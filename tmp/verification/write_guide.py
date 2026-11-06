from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'output'
tests=json.loads((ROOT/'tmp/verification/test-results.json').read_text())
# 旧结果文件中的外部Bootloader记录不参与本仓库的断言统计。
tests=[x for x in tests if x['target'] in ['main','ball']]
assert {x['target'] for x in tests}=={'main','ball'}
scene_sync='BALL_CMD_SCENE' in (ROOT/'Wusong/Task/Inc/CommTask.h').read_text(encoding='utf-8')
def frame(direction,code,data=0,expand=0,ack=0,id=1):
    b=bytes([0xAA,0,id,direction,code])+data.to_bytes(4,'big')+bytes([ack,expand])
    crc=0xFFFF
    for byte in b:
        crc^=byte
        for _ in range(8):crc=(crc>>1)^0xA001 if crc&1 else crc>>1
    return (b+crc.to_bytes(2,'big')+b'\x55').hex(' ').upper()
examples=[('安卓→主控，进入游玩场景',1,7,6,0,0),('安卓→主控，J1中奖灯',1,9,1,0,1),
          ('主控→微动板，请求版本',4,0,0,0,0),('主控→微动板，点亮J1/LED12',4,1,1,0,0),
          ('主控→微动板，挡珠阀',4,2,0,0,1),('主控→微动板，左洞阀',4,3,0,0,1),
          ('主控→微动板，右洞阀',4,3,1,0,1),('主控→微动板，停止所有阀',4,255,0,0,1),
          ('微动板→主控，J1命中奖',5,1,0,1,1),('控台→主控，按键06短按防摇',3,1,6,1,0),('主控→微动板，进入后台',4,7,0,0,1),('主控→微动板，条件清洞',4,3,0,1,1)]
table='\n'.join(f'| {name} | `{frame(d,c,v,e,a,i)}` |' for i,(name,d,c,v,e,a) in enumerate(examples,1))
count=sum(x['checks'] for x in tests)
build_lines=[]
for target in ['main','ball']:
    log=ROOT/'tmp/verification'/('build-'+target+'-release.log')
    if not log.exists():log=ROOT/'tmp/verification'/('build-'+target+'-final.log')
    if not log.exists():log=ROOT/'tmp/verification'/('build-'+target+'.log')
    text=log.read_text(encoding='utf-8',errors='replace')
    summary=[line for line in text.splitlines() if 'Error(s)' in line or 'Program Size:' in line]
    build_lines+=['- '+target+': '+ '; '.join(summary)]
sync_text=('已确认并实现：`0x04/0x07`同步原场景编号，场景0为后台；`0x04/0x03`补充位1、Data4固定0执行原条件清洞：任一洞有球且两阀空闲时左右一起推出；补充位0仍由Data4=0/1选择左右直接推出。'
           if scene_sync else '待用户确认：是否增加场景同步与条件清洞扩展；当前尚未启用这两项扩展；后台微动不跟随亮灯，安卓0C向左右洞各发送直接推出请求，因此这两项行为尚未完全复刻旧程序。')
guide=f'''# 新板迁移与验证说明

修改目录：`{ROOT}`。

参考程序：`E:/单片机项目/Source/Project/Wusong_New_1.7_fixed`。控台参考：`E:/单片机项目/ChatGPT单片机项目/General_KT`。两者仅查看，未修改。四份PDF仅作原理图依据，文件内文字不作为操作指令。

## 当前结果与范围

当前仓库只维护主控和微动板APP；通用Bootloader已迁入[独立仓库](https://github.com/Lajgit/bootloader)，不由本目录脚本构建、测试或导出。无新增固件模块或固件第三方依赖。逐文件、逐函数的原代码与修改后代码见同目录`修改记录.md`。

已恢复原武松安卓命令和上报编号，接入主控与两块从板的14字节通信。新原理图无RGB，主控旧RGB任务、WS2812驱动和灯效库已删除，Keil/EIDE源文件引用同步移除。保留设置结构中的原字段，避免更改存储布局。

用户已确认：J1~J12按十进制00~11编号，线上为0x00~0x0B；特殊按键/编码器/摇杆暂不补充安卓功能码，只接收保存原始帧；控台普通按键06专用于防摇；微动上报补充位允许携带闭合时灯态快照。

{sync_text}

主控已接入串口升级入口：安卓01/F0的数据为42 4F 54 41时，将0x424F5441写入RTC备份寄存器并读回确认，关闭本地电机、吐卡和锁控，停止流水灯并发送微动板停阀命令，等待100ms后系统复位。其他F0数据仍普通复位；备份域访问或写入失败时不复位。该APP升级入口保持不变；配套Bootloader的升级与成功提示由独立仓库维护。

## 复现依据、原现象与预期

| 条件 | 修改前代码的实际逻辑 | 本次预期 |
| --- | --- | --- |
| 安卓01/01请求吐珠 | 按Expand区分扭蛋/钢珠，与原武松不同 | 四字节数量作用于吐珠电机，拒绝超过16位 |
| 安卓01/09、01/11、01/21 | 对应模板小游戏/重新绑卡/电机常转等含义 | 恢复通道灯、默认设置、连续流水灯 |
| 原武松吐卡一次/完成 | 当前模板发送05剩余卡数/11关闭提示 | 恢复00/14和00/15，Data=0、Expand=0 |
| 微动板接新原理图 | 模板使用PA2/PA3串口、PA6 RGB、七路光眼 | USART1 PA9/PA10，十二路微动、SM16306和三阀 |
| 主控接PD5/PD6 | 没有USART2接收任务 | USART2中断接收并处理微动板消息 |

以上是代码审查和仿真复现，不是实物现象。实物烧录、联机、实际机械时序、噪声条件和断电过程均**尚未验证**。

## 硬件映射

| 链路 | 主控端 | 从板端 | 参数 |
| --- | --- | --- | --- |
| 安卓 | USART1 PA9 TX / PA10 RX | 安卓串口 | 115200 8N1 |
| 控台小卡 | USART3 PB10 TX / PB11 RX | USART2 PA2 TX / PA3 RX | 115200 8N1 |
| 球盘微动板 | USART2 PD5 TX / PD6 RX | USART1 PA9 TX / PA10 RX | 115200 8N1 |

TX接对端RX，RX接对端TX，共地。

| 微动接口 | 上报Data4 | MCU输入 | 对应LED |
| --- | --- | --- | --- |
| J1 | 00 | PA3 | LED12 |
| J2 | 01 | PA4 | LED11 |
| J3 | 02 | PA5 | LED10 |
| J4 | 03 | PA6 | LED9 |
| J5 | 04 | PA7 | LED8 |
| J6 | 05 | PA8 | LED7 |
| J7 | 06 | PA11 | LED6 |
| J8 | 07 | PA12 | LED5 |
| J9 | 08 | PA15 | LED4 |
| J10 | 09 | PB3 | LED3 |
| J11 | 0A | PB4 | LED2 |
| J12 | 0B | PB5 | LED1 |

通道bit0对应J1，bit11对应J12；SM16306物理OUT位序由微动板反转，主控不重复反转。PB15=SDI、PB14=CLK、PB13=LE，OE#硬接地。微动闭合低有效，15ms消抖，每次闭合只报一次。

左洞阀=PB10、右洞阀=PB11、挡珠阀=PB0，高有效；左洞口=PB1、右洞口=PB2，沿用原逻辑高有效并连续确认200ms。微动板指示灯PA1低有效。关闭JTAG释放PA15/PB3/PB4，保留SWD。

挡珠阀每次吸合800ms。原武松左右洞的TriggerCount=1实际执行两次吸合，迁移保留800ms吸合、300ms间隔、800ms吸合。时序由微动板本地任务执行，主控不通过串口发送关断时刻。

主控吐珠通道TIM1_CH1/CH2对应PE9/PE11；由于新板BI/FI通道顺序与旧板不同，方向参数保留0以使BI侧导通，速度恢复原100%。真实电机方向、速度及最后一颗计数仍需按下方步骤上板确认。

## 通信格式与业务

固定14字节：`AA RetryID ID Code1 Code2 Data1 Data2 Data3 Data4 ACK Expand CRC_H CRC_L 55`。Data大端；CRC16覆盖前11字节，初值FFFF，多项式A001，**CRC高字节先发送**，与现有控台源码一致。

ACK=1时原帧原方向回显。发送端每1000ms重发一次，RetryID递增，最多重发3次；接收端保留4500ms的已处理ID用于拒绝同ID重传。主控按保存的原串口重传，避免把球盘动作重发给安卓。

| 方向/功能 | Code1/Code2 | 数据 |
| --- | --- | --- |
| 主控请求控台版本 / 数码管 | 02/00、02/01 | 版本请求Data=0；数码管四字节原值 |
| 控台版本、普通按键、特殊键、编码器、摇杆 | 03/00~04 | 按用户表接收；普通键1~5转原安卓03；6转防摇 |
| 主控请求微动版本 | 04/00 | Data=0 |
| 主控通道灯 | 04/01 | Data1/2=0，Data3/4=16位掩码，只有低12位有效 |
| 主控挡珠阀 | 04/02 | Data=0，ACK=1 |
| 主控洞内阀 | 04/03 | Expand=0时Data4=0左、1右直接推出；Expand=1且Data4=0时条件成对清洞，ACK=1 |
| 主控场景同步 | 04/07 | Data4=原场景0~11，Expand=0，ACK=1；新场景取消旧场景待重发帧 |
| 主控停止球盘输出（补齐原安卓FF） | 04/FF | 三阀立即关闭并取消后续脉冲；主控先清除待重发的旧阀命令 |
| 微动板版本 | 05/00 | 四字节版本 |
| 微动编号 | 05/01 | Data4=00~0B，ACK=1；Expand=0未中奖、1中奖（闭合时快照） |
| 洞口 | 05/02 | Data4=0左、1右，ACK=1，Expand=0 |

主控启动和安卓场景变化时同步微动板场景。场景0中，按住微动就点亮对应灯，松开即灭；只在灯态变化时移位。后台测试灯态不会覆盖游戏掩码，离开后台恢复最后一次主控通道掩码；同场景重复同步不清灯。

安卓0C恢复输出和空闲场景自动清洞均使用补充位1条件清洞；安卓0F继续分别发送左右直接推出命令。条件清洞为空、单洞占用、双洞占用、任一阀忙均按原MiniGameBall_EjectIfOccupied函数判断，空洞不空打、忙时不重置脉冲计时。

主控只有在原SCENE_PLAYING=6或SCENE_ADDITIONAL=5时转发微动编号和输赢，分别沿用安卓0E、16/17。流水灯命令保持原`01/21 Data1=01 Data2=01 Data3=1~3 Data4=0~3 Expand=0`，周期为100/200/300/400ms；相同参数重复下发不重置起点，允许先配置再进入游玩场景。

防摇保持安卓`00/18`：首次收到控台06短按/长按事件，Data4=0触发；之后事件刷新静默计时，连续5000ms没有新的触发事件后Data4=1解除。松开帧不会触发报警。现有General_KT短按在释放时报告、长按周期报告，本次不修改小卡，因而防摇触发时间取决于其事件到达时间。

## 可直接使用的测试帧

表中每行ID不同；重复执行机械命令时应换ID，模拟重传时保留ID并递增RetryID后重算CRC。下行帧发到接收方串口；微动板上行帧只用于主控逻辑注入测试。

| 目的 | 完整十六进制帧 |
| --- | --- |
{table}

## 编译、固件及烧录

本机Keil路径`E:/Keil5/UV4/UV4.exe`。主控APP链接`0x0800C000`，显式VTOR偏移C000；设置区为`0x080E0000`。外部通用Bootloader从`0x08000000`启动，编译时选择`BOOT_BOARD_WUSONG`。本仓库不再合并或导出Bootloader镜像。

PowerShell重编译、回归与导出当前两个APP：

```powershell
Set-Location -LiteralPath '{ROOT}'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./tmp/verification/build_release.ps1
python ./tmp/verification/run_tests.py
python ./tmp/verification/export_firmware.py
python ./tmp/verification/write_report.py
python ./tmp/verification/write_guide.py
```

首次运行仿真需按下方命令准备本地依赖；Keil及Python路径需与本机安装一致。构建脚本仍生成临时Keil工程，使用独立输出目录并在结束后移除临时工程，不更改正式工程设置。

实际编译记录（完整日志在tmp/verification）：

{chr(10).join(build_lines)}

已有编译警告主要是未改动文件的结尾换行、未使用旧变量、旧枚举混用，未为消警告修改无关代码。

本次导出清单仅包含`Wusong_Main.hex`、`Wusong_Ball.hex`和`wusong.bin`。manifest.json记录三份文件的长度、SHA256、主控向量及当前两工程源码摘要，导出脚本验证地址范围和主控HEX/BIN一致。目录中旧的Bootloader镜像、合并镜像及旧ZIP不会纳入新清单，本次未删除这些本地历史产物。

首次安装时先从通用Bootloader仓库编译取得`Bootloader_Wusong.hex`，再分别烧写主控APP和微动板；不要使用旧的合并镜像代替当前产物。以下为待执行步骤，本次未连接或写入实物：

```powershell
$taskProgrammer = 'D:/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe'
# 先在通用Bootloader仓库选择武松板型并编译，确认此文件来自本次构建。
$taskBootHex = 'E:/单片机项目/ChatGPT单片机项目/通用bootloader/Bootloader/MDK-ARM/firmware/Bootloader_Wusong.hex'
if (-not (Test-Path -LiteralPath $taskBootHex -PathType Leaf)) {{ throw 'Build the external Wusong Bootloader first' }}
# SWD连接主控板：首次安装Bootloader；已有匹配版本时可跳过这一条。
& $taskProgrammer -c port=SWD mode=UR freq=1000 -w $taskBootHex -v -rst
& $taskProgrammer -c port=SWD mode=UR freq=1000 -w '{OUT/'firmware/Wusong_Main.hex'}' -v -rst
# SWD改接微动板，复核连接后执行。
& $taskProgrammer -c port=SWD mode=UR freq=1000 -w '{OUT/'firmware/Wusong_Ball.hex'}' -v -rst
```

控台小卡不烧写、不修改。主控SD升级时，将当前`wusong.bin`放入SD根目录后重启，成功后移走文件，避免后续启动重复升级。

配套Bootloader串口协议为`AA 5A`、版本1、CRC32、BOTA标识，单块最多1024字节。APP进入命令沿用14字节安卓帧：Code1=01、Code2=F0、Data1~4=42 4F 54 41；数据和CRC16高字节在前，应答位可设1。其他F0数据仍普通复位。缓存上限261888字节；本仓库的仿真仅验证APP升级入口，不替代独立仓库的Bootloader验证。

## 主控串口升级入口与上板验证

本次在主控既有CommTask模块中修改两处：CommTask.h增加OTA_REQUEST_MAGIC；CommTask.c新增Board_EnterBootloader，并将USART1_Deal中的F0由直接System_Reset改为按四字节标记分流。完整原代码/修改后代码、原因和影响范围见修改记录。VERSION仍为0x01000400，未调整其他安卓命令和板间协议。

软件回归新增普通复位、错误升级标记、备份域访问失败、有效升级请求的标记读回/关闭输出/100ms等待/发送顺序，以及坏CRC和错误方向检查。仿真仅模拟备份域可写和不可访问；真实RTC保持、供电、电机停机、串口收发与安装断电恢复均尚未验证。请求回显只代表收到帧，是否进入升级须以Bootloader HELLO应答确认。

先按上方步骤准备独立通用Bootloader和当前APP，并分别烧入主控；首次安装需要SWD，已有配套Bootloader时也可通过SD安装本版APP。微动板继续使用已有程序。USB转串口TX接主控PA10、RX接PA9并共地，使用匹配主板逻辑电平的接口；升级期间由工具独占USART1，暂停安卓端通信。测试时移走SD根目录的升级文件，避免复位后再次触发SD升级。

使用参考仓库固定提交内的1.2-fixed工具，以下COM4须替换为实际串口号：

```powershell
$taskOta = Join-Path $env:TEMP 'wusong_serial_ota.py'
Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/Lajgit/DANJIE_WIFI/cf3fa5f406bfefa63bb60aeea5ef75e3664fe9d8/%E4%B8%B2%E5%8F%A3%E5%8D%87%E7%BA%A7%E6%B5%8B%E8%AF%95/danjie_serial_ota.py' -OutFile $taskOta
py -m pip install pyserial
py $taskOta version --port COM4
# 先验证能进入Bootloader，再执行完整升级；使用本机生成的武松BIN。
py $taskOta bootloader --port COM4
py $taskOta upgrade 'E:/单片机项目/ChatGPT单片机项目/新弹珠机/output/firmware/wusong.bin' --port COM4 --target-version 0x01000400
```

升级后应重新查询到0x01000400，保存的设置应保留；再按下方步骤回归原安卓、控台和微动板功能。工具的BOTA目标标识不区分机型，不要用参考仓库的Danjie测试BIN替代wusong.bin。本次未连接或烧录实物。

## 配套通用Bootloader

升级成功后的微动板12路灯全亮一秒功能位于独立[通用Bootloader仓库](https://github.com/Lajgit/bootloader)，需要选择武松板型并通过SWD更新Bootloader。本仓库只保留能接收对应场景/通道灯命令的微动板APP，以及主控F0进入升级入口。

成功灯效、SD安装、串口INSTALL和断电恢复需用独立仓库的Bootloader另行验证；本仓库当前测试不再编译已迁出的实现。已有Flash设置字段和0x0800C000 APP布局仍然保留。

## 已执行测试与回归步骤

已执行ARM指令级仿真，直接编译并包含被测生产C文件；GPIO、UART及备份寄存器访问由测试桩或仿真内存提供。共{count}项断言通过：{', '.join(x['target']+' '+str(x['checks']) for x in tests)}。测试结果见`tmp/verification/test-results.json`，编译和链接日志同目录。此结果验证软件分支、报文和Flash地址逻辑，不代表真实电气和机械性能。

重跑命令（依赖只放在tmp/verification/runtime，不进入固件工程）：

```powershell
Set-Location -LiteralPath '{ROOT}'
$taskPython = 'python'
& $taskPython -m pip install --target '{ROOT}/tmp/verification/runtime' unicorn==2.1.4 pyelftools==0.33
& $taskPython '{ROOT}/tmp/verification/run_tests.py'
```

实物回归均**尚未验证**，依次执行：

1. 上电读取控台、微动板版本，确认微动板收到初始场景同步；确认两条从板串口均有正确CRC帧，三个阀上电关闭。
2. 游玩场景下逐个按J1~J12，确认编号00~0B，按住不重复上报；单独设置掩码0001/0800确认LED12/LED1。进入后台场景0后，逐个压住并松开微动，确认对应LED跟随；在后台改变游戏掩码再离开，确认恢复游戏掩码。
3. 下发中奖通道后触发对应/不对应微动，确认安卓先收到16/17，再收到0E编号。流水灯临界时触发，检查上报Expand使用闭合瞬间快照，重传快照不变。
4. 检查挡珠800ms；左、右阀独立800/300/800ms。丢弃应答模拟重传，阀不重复启动；FF后不再出现旧阀补发。
5. 两侧洞口连续有效不足200ms不上报，达到200ms只报一次，解除后再触发可再次上报。安卓0C分别测试两洞都空、仅左有球、仅右有球及两洞都有球：都空不动作，任一有球且两阀空闲时成对推出；任一阀忙时不能启动另一侧或重置当前时序。空闲场景自动清洞也执行相同条件判断。
6. 控台普通键1~5对照安卓03；控台06触发18/00，重复触发不重复新报警，5秒静默后18/01；特殊键、编码器、摇杆只更新接收状态。
7. 吐珠、投币、出卡、恢复输出、停止、开锁和后台按键逐项与旧安卓联调；核实05剩余珠数Expand=0、14吐卡一次、15吐卡完成。实际吐珠方向、最后一颗停机和计数须上板确认。
8. 检查保存0E、恢复11与重启后的设置；F0应普通重启。
9. SD依次测试无文件、有效新BIN、旧04000地址BIN、错误文件、升级中断电、升级成功后移除BIN。验证Boot、设置区保留。此处为配套通用Bootloader的实机回归步骤，本仓库当前脚本不验证其安装实现。
10. 按“主控串口升级入口与上板验证”章节从APP进入Bootloader并执行HELLO/BEGIN/DATA/END/INSTALL；普通F0应继续正常重启。校验错误标记、错误CRC及错误方向不会误进升级，升级前电机/吐卡/锁控关闭、三只微动板阀停止。重复块、非法大小和目标、安装断电恢复需逐项实测。

## 此前脚本清理记录

通用Bootloader迁出后，原构建/回归/导出入口仍访问已删除目录，报告生成器也把已删除文件纳入统计。此前脚本清理仅调整验证入口和生成说明；该次未修改业务源码或删除本地历史固件、临时目录。

| 文件/模块 | 原行为 | 修改后及影响 |
| --- | --- | --- |
| build_release.ps1 / 目标列表 | 构建三个工程，第三个路径已删除 | 仅构建主控/微动板，目录清单只写两项 |
| run_tests.py / 回归循环 | 编译并执行本地Boot测试 | 仅回归主控/微动板；保留APP备份寄存器入口测试 |
| boot_test.c / 旧Boot测试入口 | 直接包含已迁出的实现 | 从本仓库删除，Boot由独立仓库验证 |
| export_firmware.py / 导出与清单 | 复制Boot并生成合并镜像，枚举目录所有固件 | 仅导出三份当前APP产物；导出前检查源码时间，旧文件不混入清单 |
| write_report.py / 输入选择 | 基线差异包含已删除Boot及SDK | 仅选择两个工程；本次补充RGB删除记录及工程配置差异，共49个路径 |
| write_guide.py / 日志与模板 | 使用本地Boot日志及合并烧录步骤 | 仅统计两APP，说明单独获取通用Bootloader的流程 |

以上脚本的具体原代码与修改后代码以本次Git提交差异为准；本节给出变更原因、模块及影响范围。脚本按上方顺序运行；当前软件验证结果见前文，真实烧录和机械回归仍未执行。

## 主控RGB与第二路出珠清理

复现条件：原模板仍将LightTask、port_light、port_lighteffect和app_ws2812列入主控工程；原Motor_Hoolle2每轮执行扭蛋状态机，PE4边沿调用扭蛋计数并与第一路共用TIM7。预期：删除无RGB硬件对应的灯效实现；第二路作为出珠预留，当前不调用且不影响第一路。

- 删除主控上述四组C/H文件（共8个），移除Wusong.uvprojx、Wusong.uvoptx和.eide/eide.yml中的四个C文件引用；微动板12路通道灯、主板数码管、系统指示灯继续保留。
- CtrlTask.c保留第二路电机初始化到断电空闲状态，注释第二路Ctrl_HoolleMotor调用。InterruptTask.c中第二路预留实现改用与第一路一致的2ms脉宽门槛及末颗停机处理，但HAL_GPIO_EXTI_Callback的PE4分支不调用它，不再改写共享TIM7。
- MesgTask.h两项Egg事件改为第二路出珠事件名，位号保持2/3，当前不接入安卓上报。现有安卓出珠、清珠、恢复输出仍只调用第一路；没有新增第二路启动协议。
- 清理电子锁错误机型、旧灯效已删除等历史注释，修正手动电机函数的0x21说明；实际0x21继续用于武松通道流水灯。舵机初始化保持当前实现。

逐文件绝对路径、模块/函数、原因、影响和原/修改后代码见同目录`修改记录.md`；本次准确差异见本地两次功能提交。EIDE仅核对引用，未实际执行EIDE构建；Keil及ARM仿真结果见上方记录。

在上方完整重编译、回归和导出命令通过后，按烧录步骤更新主控APP，再执行以下实机回归（尚未验证）：

1. 上电检查第二路电机保持断电，主控系统灯、数码管及球盘通道灯正常。
2. 发送安卓01/01出珠请求、0B清珠、0C恢复输出；只允许第一路执行，第二路不得启动。
3. 第一路持续出珠时切换PE4高/低电平，确认第一路计数和末颗停机不受影响；第二路不产生剩余数量或超时上报。
4. 回归球盘流水灯/中奖快照、三阀、控台输入、吐卡、开锁及F0升级入口；SD/串口安装由独立通用Bootloader执行。

## 本地Git提交与中文注释

迁移代码按通信接收修复、主控硬件适配、微动板业务、主控通信与安卓兼容、Bootloader兼容、验证资料与说明六类分批提交，提交说明使用中文。关键新增逻辑补充中文注释，并校正旧“四路按键”“80%速度”等与现状不符的注释；注释补充未改变生产C代码的词法记号。

仓库收录生产源码、两份迁移说明，以及tmp/verification中明确选取的可复现验证源码和脚本。生成的HEX/BIN/AXF、编译中间文件、仿真依赖包、临时原理图图片、日志和ZIP不在本次提交范围内。

```powershell
git log --oneline --reverse 8ecfd8c..HEAD
git status --short
```

## 仍需注意的工程边界

临时图片、历史构建目录和旧固件包不在本次脚本清理范围，均继续由现有忽略规则屏蔽。

仅同步本次相关的CubeMX引脚与外设配置；主控IOC仍保留原模板其他外设项，本次未重新生成整个工程。Keil源文件为当前构建依据；不要直接让CubeMX覆盖本次手工业务迁移代码。

本次按要求删除主控旧RGB专用模块并调整第二路出珠预留逻辑；舵机初始化、标准库和其他无关模板代码保留。微动板环形缓冲满时被误判为空的问题已由测试复现，并仅在微动板接收依赖库中修正。具体生产修改范围严格以`修改记录.md`为准。
'''
(OUT/'迁移说明.md').write_text(guide,encoding='utf-8')
print(json.dumps({'guide_bytes':(OUT/'迁移说明.md').stat().st_size,'assertions':count,'scene_sync':scene_sync}))
