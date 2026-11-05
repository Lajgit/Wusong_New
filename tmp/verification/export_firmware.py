# 从已验证构建导出固件，并核对地址范围、向量、HEX/BIN一致性和源码摘要。
from pathlib import Path
import subprocess,json,hashlib,struct,shutil
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'output'/'firmware'
OUT.mkdir(parents=True,exist_ok=True)
artifacts={
 'Wusong_Main.hex':ROOT/'Wusong/MDK-ARM/Wusong/Wusong.hex',
 'Wusong_Ball.hex':ROOT/'Wusong_Ball/MDK-ARM/Wusong_Ball/Wusong_Ball.hex',
 'Wusong_Bootloader.hex':ROOT/'Bootloader_Danjie/MDK-ARM/Bootloader_F407/Bootloader_F407.hex'}
catalog_path=ROOT/'tmp/verification/release-builds.json'
catalog=json.loads(catalog_path.read_text(encoding='utf-8-sig')) if catalog_path.exists() else None
if catalog:
    artifacts={name:Path(catalog[target]['hex']) for name,target in [('Wusong_Main.hex','main'),('Wusong_Ball.hex','ball'),('Wusong_Bootloader.hex','boot')]}
for name,source in artifacts.items():shutil.copy2(source,OUT/name)
subprocess.run(['E:/Keil5/ARM/ARMCC/bin/fromelf.exe','--bin','--output',str(OUT/'wusong.bin'),str(Path(catalog['main']['axf']) if catalog else ROOT/'Wusong/MDK-ARM/Wusong/Wusong.axf')],check=True)

def parse(path):
    data={};base=0
    for line in path.read_text().splitlines():
        record=bytes.fromhex(line[1:]);assert sum(record)&255==0
        n,address,kind=record[0],int.from_bytes(record[1:3],'big'),record[3]
        if kind==0:
            for i,value in enumerate(record[4:4+n]):
                dest=base+address+i
                assert dest not in data
                data[dest]=value
        elif kind==4:base=int.from_bytes(record[4:6],'big')<<16
        elif kind==2:base=int.from_bytes(record[4:6],'big')<<4
        elif kind==1:break
    return data

main=parse(OUT/'Wusong_Main.hex');boot=parse(OUT/'Wusong_Bootloader.hex');ball=parse(OUT/'Wusong_Ball.hex')
assert min(main)==0x0800C000 and max(main)<0x080A0000
assert min(boot)==0x08000000 and max(boot)<0x0800C000
assert min(ball)==0x08000000 and max(ball)<0x08010000
binary=(OUT/'wusong.bin').read_bytes()
sp,entry=struct.unpack_from('<II',binary)
assert 0x20000000<=sp<0x20020000 and entry&1 and 0x0800C000<=entry&~1<0x080A0000
assert all(binary[address-0x0800C000]==value for address,value in main.items())
assert len(binary)<0x3FF00

def record(address,kind,payload):
    raw=bytes([len(payload)])+address.to_bytes(2,'big')+bytes([kind])+payload
    return ':'+(raw+bytes([-sum(raw)&255])).hex().upper()

merged={**boot,**main};addresses=sorted(merged);lines=[];i=0;upper=None
while i<len(addresses):
    address=addresses[i];high=address>>16
    if high!=upper:lines.append(record(0,4,high.to_bytes(2,'big')));upper=high
    payload=bytearray([merged[address]]);i+=1
    while i<len(addresses) and len(payload)<16 and addresses[i]==address+len(payload) and addresses[i]>>16==high:
        payload.append(merged[addresses[i]]);i+=1
    lines.append(record(address&65535,0,bytes(payload)))
lines.append(record(0,1,b''))
(OUT/'Wusong_Factory.hex').write_text('\n'.join(lines)+'\n',encoding='ascii')
assert parse(OUT/'Wusong_Factory.hex')==merged
metadata={
 'application_address':'0x0800C000','settings_address':'0x080E0000',
 'stack_pointer':hex(sp),'reset_handler':hex(entry),'board_id':4,
 'firmware_files':{p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(OUT.iterdir()) if p.suffix in ['.hex','.bin']}}
source_files=subprocess.check_output(['git','-c','core.safecrlf=false','diff','--name-only','-z','8ecfd8c'],cwd=ROOT).decode('utf-8').split('\0')
source_files=[p for p in source_files if p.startswith(('Wusong/','Wusong_Ball/','Bootloader_Danjie/'))]
metadata['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in source_files if Path(p).suffix in ['.c','.h','.ioc','.uvprojx']}
if catalog:
    for target,prefix in [('main','Wusong/'),('ball','Wusong_Ball/'),('boot','Bootloader_Danjie/')]:
        inputs=[ROOT/p for p in source_files if p.startswith(prefix) and Path(p).suffix in ['.c','.h','.uvprojx']]
        assert all(p.stat().st_mtime <= Path(catalog[target]['axf']).stat().st_mtime for p in inputs), 'Source newer than compiled firmware: '+target
    metadata['build_logs']={target:entry['log'] for target,entry in catalog.items()}
(OUT/'manifest.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
print(json.dumps({name:info['bytes'] for name,info in metadata['firmware_files'].items()}))
