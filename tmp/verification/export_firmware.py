# 从已验证构建导出固件，并核对地址范围、向量、HEX/BIN一致性和源码摘要。
from pathlib import Path
import subprocess,json,hashlib,struct,shutil
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'output'/'firmware'
OUT.mkdir(parents=True,exist_ok=True)
artifacts={
 'Wusong_Main.hex':ROOT/'Wusong/MDK-ARM/Wusong/Wusong.hex',
 'Wusong_Ball.hex':ROOT/'Wusong_Ball/MDK-ARM/Wusong_Ball/Wusong_Ball.hex'}
catalog_path=ROOT/'tmp/verification/release-builds.json'
catalog=json.loads(catalog_path.read_text(encoding='utf-8-sig')) if catalog_path.exists() else None
if catalog:
    artifacts={name:Path(catalog[target]['hex']) for name,target in [('Wusong_Main.hex','main'),('Wusong_Ball.hex','ball')]}
# 只对当前两工程收集源码摘要；排除已迁出或删除的路径。
source_files=subprocess.check_output(['git','-c','core.safecrlf=false','diff','--diff-filter=ACMRT','--name-only','-z','8ecfd8c','--','Wusong/','Wusong_Ball/'],cwd=ROOT).decode('utf-8').split('\0')
source_files=[p for p in source_files if Path(p).suffix in ['.c','.h','.ioc','.uvprojx']]
if catalog:
    for target,prefix in [('main','Wusong/'),('ball','Wusong_Ball/')]:
        inputs=[ROOT/p for p in source_files if p.startswith(prefix) and Path(p).suffix in ['.c','.h','.uvprojx']]
        assert all(p.stat().st_mtime <= Path(catalog[target]['axf']).stat().st_mtime for p in inputs), 'Source newer than compiled firmware: '+target
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

main=parse(OUT/'Wusong_Main.hex');ball=parse(OUT/'Wusong_Ball.hex')
assert min(main)==0x0800C000 and max(main)<0x080A0000
assert min(ball)==0x08000000 and max(ball)<0x08010000
binary=(OUT/'wusong.bin').read_bytes()
sp,entry=struct.unpack_from('<II',binary)
assert 0x20000000<=sp<0x20020000 and entry&1 and 0x0800C000<=entry&~1<0x080A0000
assert all(binary[address-0x0800C000]==value for address,value in main.items())
assert len(binary)<0x3FF00
metadata={
 'application_address':'0x0800C000','settings_address':'0x080E0000',
 'stack_pointer':hex(sp),'reset_handler':hex(entry),'board_id':4,
 # 显式列出本次产物，目录里保留的旧Boot/合并镜像不进入新清单。
 'firmware_files':{name:{'bytes':(OUT/name).stat().st_size,'sha256':hashlib.sha256((OUT/name).read_bytes()).hexdigest()} for name in ['Wusong_Main.hex','Wusong_Ball.hex','wusong.bin']}}
metadata['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in source_files}
if catalog:
    metadata['build_logs']={target:catalog[target]['log'] for target in ['main','ball']}
(OUT/'manifest.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
print(json.dumps({name:info['bytes'] for name,info in metadata['firmware_files'].items()}))
