# 编译实际ARM业务源码，在仿真器中读取断言结果；GPIO/UART/Flash由C测试桩提供。
from pathlib import Path
import sys,subprocess,json
sys.stdout.reconfigure(encoding="utf-8")
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE/'runtime'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_INTR
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR
from elftools.elf.elffile import ELFFile

results=[]
# 只回归本仓库的主控和微动板，通用Bootloader由独立仓库验证。
for name,project,port,cpu,chip in [('main','Wusong','Port','Cortex-M4','STM32F407xx'),('ball','Wusong_Ball','Mid','Cortex-M3','STM32F103xB')]:
    includes=[f'{project}/Core/Inc',f'{project}/Task/Inc',f'{project}/{port}/Inc',f'{project}/Lib/Inc',
              f'{project}/Drivers/STM32{ "F1" if name=="ball" else "F4" }xx_HAL_Driver/Inc',
              f'{project}/Drivers/CMSIS/Include',f'{project}/Drivers/CMSIS/Device/ST/STM32{ "F1" if name=="ball" else "F4" }xx/Include']
    obj=HERE/(name+'_test.o');elf=HERE/(name+'_test.axf')
    command=['E:/Keil5/ARM/ARMCC/bin/armcc.exe','--cpu',cpu,'--fpu=SoftVFP','--c99','--split_sections','-O1','-D'+chip,'-DUSE_HAL_DRIVER']
    for inc in includes:command+=['-I',str(ROOT/inc)]
    command+=['-c',str(HERE/(name+'_test.c')),'-o',str(obj)]
    p=subprocess.run(command,cwd=ROOT,capture_output=True)
    (HERE/(name+'_test_compile.log')).write_bytes(p.stdout+p.stderr)
    if p.returncode: print((p.stdout+p.stderr).decode('utf-8',errors='replace'));raise SystemExit(p.returncode)
    command=['E:/Keil5/ARM/ARMCC/bin/armlink.exe',str(obj),'--entry=test_main','--ro_base=0x08000000','--rw_base=0x20000000','--libpath=E:/Keil5/ARM/ARMCC/lib','--output='+str(elf)]
    p=subprocess.run(command,cwd=ROOT,capture_output=True)
    (HERE/(name+'_test_link.log')).write_bytes(p.stdout+p.stderr)
    if p.returncode:print((p.stdout+p.stderr).decode('utf-8',errors='replace'));raise SystemExit(p.returncode)
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    uc.mem_map(0x08000000,0x100000);uc.mem_map(0x20000000,0x20000);uc.mem_map(0x40000000,0x100000)
    # RCC的RTC时钟使能使用位带别名；映射该页以执行真实寄存器写入指令。
    uc.mem_map(0x42470000,0x1000)
    symbols={}
    with elf.open('rb') as stream:
        e=ELFFile(stream)
        for seg in e.iter_segments():
            if seg['p_type']=='PT_LOAD':uc.mem_write(seg['p_vaddr'],seg.data())
        # 按执行地址装载初始化数据，仿真入口不经过芯片启动文件的数据搬运。
        for section in e.iter_sections():
            if section['sh_flags'] & 2 and section['sh_type']=='SHT_PROGBITS':
                uc.mem_write(section['sh_addr'],section.data())
        table=e.get_section_by_name('.symtab')
        for sym in table.iter_symbols():symbols[sym.name]=sym['st_value']
        entry=e.header['e_entry']
    uc.reg_write(UC_ARM_REG_SP,0x2001FFF0)
    uc.reg_write(UC_ARM_REG_LR,0x080FFFF1)
    # 只有到达测试末尾BKPT才算完成；异常或指令数耗尽不能误报通过。
    stop_interrupts=[]
    def stop(emu,number,data):stop_interrupts.append(number);emu.emu_stop()
    uc.hook_add(UC_HOOK_INTR,stop)
    uc.emu_start(entry|1,0,count=5000000)
    values={key:int.from_bytes(uc.mem_read(symbols[key],4),'little') for key in ['checks','failed_line']}
    result={'target':name,**values,'passed':values['failed_line']==0 and values['checks']>10 and stop_interrupts==[7], 'stop_interrupts':stop_interrupts}

    if values['failed_line']:
        count=int.from_bytes(uc.mem_read(symbols['sent_count'],4),'little') if 'sent_count' in symbols else 0
        result['frames']=[bytes(uc.mem_read(symbols['sent']+i*14,14)).hex() for i in range(count)]
    results.append(result);print(json.dumps(result))
(HERE/'test-results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
if not all(r['passed'] for r in results):raise SystemExit(1)
