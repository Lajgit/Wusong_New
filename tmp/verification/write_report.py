from pathlib import Path
import subprocess,json,hashlib,re
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'output'
OUT.mkdir(exist_ok=True)
def git(*args):return subprocess.check_output(['git','-c','core.quotepath=false','-c','core.safecrlf=false',*args],cwd=ROOT).decode('utf-8')
files=[p for p in git('diff','--diff-filter=ACMRTD','--name-only','8ecfd8c','--','Wusong/','Wusong_Ball/').splitlines() if p.startswith(('Wusong/','Wusong_Ball/')) and Path(p).suffix in ['.c','.h','.ioc','.uvprojx','.uvoptx','.yml']]
details={
'Wusong/Task/Inc/InterruptTask.h':('输入初始化契约 / HoolleInput_FilterInit','声明该函数必须在Device_Init后调用，原子清待报计数并开放输入处理。','仅初始化调用契约说明；线上协议不变。'),
'Wusong/MDK-ARM/.eide/eide.yml':('EIDE / virtualFolder', '删除已移除RGB任务、WS2812驱动与灯效库的源码引用。', '仅主控EIDE源文件列表，未实测EIDE构建。'),
'Wusong/MDK-ARM/Wusong.uvoptx':('Keil选项 / File列表', '删除旧RGB四个C文件的调试记录，避免项目打开后出现失效路径。', '仅主控调试文件记录。'),
'Wusong/MDK-ARM/Wusong.uvprojx':('Keil目标 / File列表', '删除旧RGB任务、WS2812驱动与灯效库的四个C文件编译引用。', '主控不再编译这些模块；球盘通道灯和数码管保留。'),
'Wusong/Task/Inc/MesgTask.h':('消息事件 / 第二路出珠事件宏', '将扭蛋剩余数量和超时事件改为第二路出珠名称，保留原位号。', '仅内部事件定义，不新增或更改线上协议。'),
'Wusong/Task/Inc/LightTask.h':('RGB/呼吸灯任务 / 整个文件', '新板无RGB且当前无外部调用，按要求删除旧灯效实现及声明。', '仅主控旧RGB专用模块；实际12路通道灯位于微动板，不受影响。'),
'Wusong/Task/Src/LightTask.c':('RGB/呼吸灯任务 / 整个文件', '新板无RGB且当前无外部调用，按要求删除旧灯效实现及声明。', '仅主控旧RGB专用模块；实际12路通道灯位于微动板，不受影响。'),
'Wusong/Port/Inc/port_light.h':('RGB灯对象 / 整个文件', '新板无RGB且当前无外部调用，按要求删除旧灯效实现及声明。', '仅主控旧RGB专用模块；实际12路通道灯位于微动板，不受影响。'),
'Wusong/Port/Src/port_light.c':('RGB灯对象 / 整个文件', '新板无RGB且当前无外部调用，按要求删除旧灯效实现及声明。', '仅主控旧RGB专用模块；实际12路通道灯位于微动板，不受影响。'),
'Wusong/Port/Inc/port_lighteffect.h':('RGB/呼吸灯效库 / 整个文件', '新板无RGB且当前无外部调用，按要求删除旧灯效实现及声明。', '仅主控旧RGB专用模块；实际12路通道灯位于微动板，不受影响。'),
'Wusong/Port/Src/port_lighteffect.c':('RGB/呼吸灯效库 / 整个文件', '新板无RGB且当前无外部调用，按要求删除旧灯效实现及声明。', '仅主控旧RGB专用模块；实际12路通道灯位于微动板，不受影响。'),
'Wusong/Lib/Inc/app_ws2812.h':('WS2812驱动 / 整个文件', '新板无RGB且当前无外部调用，按要求删除旧灯效实现及声明。', '仅主控旧RGB专用模块；实际12路通道灯位于微动板，不受影响。'),
'Wusong/Lib/Src/app_ws2812.c':('WS2812驱动 / 整个文件', '新板无RGB且当前无外部调用，按要求删除旧灯效实现及声明。', '仅主控旧RGB专用模块；实际12路通道灯位于微动板，不受影响。'),
'Wusong_Ball/Lib/Src/app_ringbuf.c':('串口环形缓冲 / RingBuffer_IsEmpty/IsFull/GetSpace/GetDatalen/ReadByte','满缓冲时索引相同却被判空，四字节容量测试已复现；改用Count判空判满，读操作短临界区与现有主控一致。','微动板串口突发接收；不改变缓冲结构或通信协议。'),
'Wusong/Core/Inc/main.h':('主控引脚宏 / PlayButton5/6、Serial2','补齐PD12/PD13两路按键，明确PD5/PD6用途。','六路直连按键和球盘串口。'),
'Wusong/Core/Inc/stm32f4xx_it.h':('中断声明 / USART2_IRQHandler','增加串口2中断声明。','主控接收球盘帧。'),
'Wusong/Core/Inc/usart.h':('串口声明 / huart2、MX_USART2_UART_Init','增加串口2句柄和初始化声明。','主控与微动板通信。'),
'Wusong/Core/Src/gpio.c':('GPIO / MX_GPIO_Init','PD12/PD13并入游玩按键输入初始化；电子锁注释去除错误机型名称，PA0逻辑不变。','六路直连按键及电子锁注释。'),
'Wusong/Core/Src/main.c':('启动 / main','初始化USART2；HAL_Init前调用SystemCoreClockUpdate，按Boot继承的实际RCC配置建立毫秒时基。','主控球盘链路及上电/Boot跳转时钟交接；SWD冷启动同样适用。'),
'Wusong/Core/Src/stm32f4xx_it.c':('中断 / USART2_IRQHandler','将串口2中断交给HAL_UART_IRQHandler。','串口2逐字节接收。'),
'Wusong/Core/Src/system_stm32f4xx.c':('向量表 / SystemInit、USER_VECT_TAB_ADDRESS、VECT_TAB_OFFSET','显式设置Flash向量偏移0xC000。','与当前Bootloader及已有链接地址一致，支持调试器和Bootloader两种启动。'),
'Wusong/Core/Src/usart.c':('串口驱动 / MX_USART2_UART_Init、HAL_UART_MspInit/DeInit','新增115200 8N1的USART2、PD5/PD6 AF7及NVIC配置。','仅增加球盘链路；USART1/3引脚不变。'),
'Wusong/Port/Src/port_communicate.c':('接收器 / Rx_Receive','固定帧长、边界检查、坏帧滑动重同步。','适配三条14字节链路，防止错误长帧复制越界。'),
'Wusong/Task/Inc/CommTask.h':('协议定义 / 方向、命令、Mesg_TypeDef、对外接口','安卓编号恢复参考武松定义；增加从板协议和状态接口。','保留14字节布局，去除其他机型的命令含义冲突。'),
'Wusong/Task/Inc/CtrlTask.h':('机构参数 / HoolleMotor_Speed','吐珠电机速度80改为100；第二路参数标注为出珠预留并暂不调用，清理扭蛋文字。','第一路出珠占空比及第二路参数注释。'),
'Wusong/Task/Inc/MainTask.h':('场景定义 / Scene_t','补齐原武松0~11场景号。','游戏、加注、后台和洞口场景判断。'),
'Wusong/Task/Src/CommTask.c':('通信任务 / USART1/2/3_Deal、Board_EnterBootloader、Ball_SetChannel/SendValve/SendScene/RequestEjectIfOccupied/StopFlow/FlowTask、CommInit/CommTask、USART_SendMesg、Resend_Task、MesgDeal_Task、CommTransmitFillData','恢复原安卓业务，接入球盘与控台、流水灯和命中快照，控台06处理防摇；新增场景同步、条件成对清洞，场景更新取消旧场景重发；每条重发帧记录原串口。F0数据为42 4F 54 41时校验RTC备份标记写入成功，关闭本地输出、发送微动板停止命令并等待100ms后复位。','主控协议与远程球盘业务；接入已有Bootloader串口升级，其他F0数据继续普通复位，备份域不可写时不复位。'),
'Wusong/Task/Src/CtrlTask.c':('机构任务 / Card_Output、CtrlTask','恢复零数量出卡完成通知，电子锁保持原2000ms；原扭蛋通道改为第二路出珠并暂停调度，保持初始化断电。清理错误机型和0x21协议注释。','第一路出珠、吐卡、开锁及舵机初始化保持；第二路未接入安卓启动命令。'),
'Wusong/Task/Src/InterruptTask.c':('接收回调 / HAL_UART_RxCpltCallback、HAL_UART_ErrorCallback','接入Rx2环形缓冲并恢复UART错误接收；第二路改为钢珠脉宽和末颗停机逻辑，禁用PE4业务入口，避免重置第一路TIM7；新增InputReady门控，设备/滤波就绪前不扫描或处理EXTI，滤波初始化原子清计数并保留PRIMASK。','串口接收及第二路预留实现；第一路计数逻辑保持。'),
'Wusong/Task/Src/KeyTask.c':('按键任务 / SettingButton_*、PlayButton_Init、KeyAll_Init','补齐六路直连按键；K3短按开锁/长按后台，K2长按4秒。','恢复参考后台按键动作并匹配新主板引脚。'),
'Wusong/Task/Src/MainTask.c':('主任务 / Main_Init','发送控台/球盘版本查询并同步初始场景，取消不存在的RGB控台命令。','上电初始化与实际从板协议匹配。'),
'Wusong/Task/Src/MesgTask.c':('安卓上报 / Mesg_Task','剩余珠数Expand=0；出卡一次/完成改为14/15；沿用参考工程未主动上报吐珠超时的选择。','保持参考安卓编号与数据布局，避免用其他机型剩余物品协议。'),
'Wusong/Wusong.ioc':('CubeMX配置 / USART2、PD5/6、PD12/13','补充USART2/NVIC与两路按键描述。','与本次新增串口和按键源码一致；未重构模板其他配置。'),
'Wusong_Ball/Core/Inc/main.h':('微动板引脚宏 / User_Led、Channel_*、Valve_*、Hole_*','按新微动板定义PA1状态灯、SM16306与三阀/两洞口。','替换不适用于本板的RGB/数码管引脚。'),
'Wusong_Ball/Core/Inc/stm32f1xx_it.h':('中断声明 / USART1_IRQHandler','USART2声明改为USART1。','微动板实际串口为PA9/PA10。'),
'Wusong_Ball/Core/Inc/usart.h':('串口声明 / huart1、MX_USART1_UART_Init','USART2句柄/接口改为USART1。','避免PA3与J1冲突。'),
'Wusong_Ball/Core/Src/gpio.c':('GPIO / MX_GPIO_Init','配置12路低有效微动、两洞口、SM16306和三阀；关闭JTAG保留SWD。','完全依据微动板引脚；阀上电低电平。'),
'Wusong_Ball/Core/Src/main.c':('启动 / main','移除RGB DMA/TIM3初始化，启用USART1。','PA6回归微动输入，PA3回归J1。'),
'Wusong_Ball/Core/Src/stm32f1xx_it.c':('中断 / SysTick_Handler、USART1_IRQHandler','移除RGB计时/DMA路径，串口中断改为USART1。','微动板有效硬件路径。'),
'Wusong_Ball/Core/Src/usart.c':('串口驱动 / MX_USART1_UART_Init、HAL_UART_MspInit/DeInit','改用USART1 PA9/PA10，115200 8N1。','匹配J14。'),
'Wusong_Ball/MDK-ARM/Wusong_Ball.uvprojx':('Keil目标 / Wusong_Ball源文件组','将dma.c、tim.c、DigitalTubeTask.c退出目标，保留原文件。','不编译不存在的硬件任务，不增加新固件文件。'),
'Wusong_Ball/Mid/Src/port_communicate.c':('接收器 / Rx_Receive','固定帧长、边界检查与坏帧滑动重同步。','与主控一致的14字节接收器；控台小卡文件未修改。'),
'Wusong_Ball/Task/Inc/CommTask.h':('协议定义 / BALL_CMD_*、BALL_REPORT_*、BallValve_Task','将球盘模板数码管/RGB定义改为通道灯、阀与微动/洞口。','落实本次微动板协议。'),
'Wusong_Ball/Task/Inc/LightTask.h':('通道灯接口 / BallLight_SetChannels、BallLight_GetChannels','改为12路通道掩码与灯态读取。','替代RGB接口，提供命中快照依据。'),
'Wusong_Ball/Task/Src/CommTask.c':('通信/阀 / USART_Deal、USART_SendMesg、Comm_SendMesg_FillData、CommInit/Task、BallValve_Start/Task/EjectIfOccupied','处理主控命令、原方向回显应答、重发去重、本地阀时序；补充位1条件清洞保留任一洞占用且两阀空闲的原判断；同步场景并隔离后台测试灯态和游戏掩码。','挡珠800ms；左右洞沿用原TriggerCount=1的800ms/300ms/800ms节奏。'),
'Wusong_Ball/Task/Src/InterruptTask.c':('UART回调 / HAL_UART_RxCpltCallback、HAL_UART_ErrorCallback','注释及错误恢复对应USART1。','字节入环形缓冲和故障后继续接收。'),
'Wusong_Ball/Task/Src/KeyTask.c':('微动/洞口 / KeyAll_Init、Key_Task','按J1~J12扫描、15ms消抖、200ms确认洞口；闭合时捕获中奖灯态；后台场景按实时微动闭合状态点亮对应灯。','每次闭合仅一次上报；补充位0/1为用户确认的快照。'),
'Wusong_Ball/Task/Src/LightTask.c':('通道灯 / Light_Init/Task、BallLight_SetChannels/GetChannels','软件移位SM16306，并将12位掩码反向映射OUT11~OUT0。','J1点亮LED12，J12点亮LED1；无RGB输出。'),
'Wusong_Ball/Task/Src/MainTask.c':('主任务 / Main_Init、Main_Task、SystemLight_Task','运行通信、输入、灯与定时阀，启用PA1板载指示灯。','替代模板RGB/数码管任务。'),
'Wusong_Ball/Wusong_Ball.ioc':('CubeMX配置 / IP、引脚、NVIC、启动列表','移除本板不具备的RGB DMA/TIM配置，配置实际微动板引脚。','与本次硬件迁移一致。'),
}
assert set(files)==set(details),(set(files)-set(details),set(details)-set(files))
# 只比较当前两个工程；包含本次删除的RGB文件，以保留完整原代码和删除依据。
lines=['# 逐文件修改记录','',
'本记录只覆盖当前主控和微动板；通用Bootloader已迁入独立仓库，不再从本目录生成其修改记录。', '',
'已按功能补充中文注释及主控F0串口升级入口；本次删除主控旧RGB专用模块，将原扭蛋通道改为第二路出珠且暂不调用。', '',
'修改基线为本目录Git初始提交8ecfd8c。功能和安卓协议依据只读参考工程 Wusong_New_1.7_fixed；原项目及General_KT均未写入。',
'', '以下每项给出绝对路径、模块/函数、原因及影响范围；diff中的减号行是原代码，加号行是修改后代码，空格行是上下文。生产源码仅修改已有文件，新增文件限于说明、输出固件和验证材料。','']
for i,path in enumerate(files,1):
    module,reason,impact=details[path]
    diff=git('diff','--no-ext-diff','--unified=3','8ecfd8c','--',path)
    lines += [f'## {i}. {path}','',f'文件：`{ROOT/path}`','',f'模块/函数：{module}。','',f'修改原因：{reason}','',f'影响范围：{impact}','', '原代码 / 修改后代码：','', '```diff',diff.rstrip(),'```','']
(OUT/'修改记录.md').write_text('\n'.join(line.rstrip() for line in '\n'.join(lines).splitlines())+'\n',encoding='utf-8')
print(json.dumps({'source_files':len(files),'change_report_bytes':(OUT/'修改记录.md').stat().st_size}))
