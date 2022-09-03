# stm32h750使用片外Flash执行程序的方法（Bootloader&Application）

## 目录

* 一、前言
* 二、程序编写
  * bootloader
  	1. 初始化QSPI
  	2. 跳转前准备
  	3. 跳转操作
  * application
    1. 工程必做的三件事
    2. 修改代码的链接地址
* 三、程序烧录
	* Openocd配置文件
	* 编辑makefile

* 四、跳转失败问题总结

* 五、结语


### 一、前言

被这个bootloader折磨好久了，从寒假到现在差不多得有4个月。一开始接触到bootloader是在rt-thread，他们做了一款开发板ART-Pi，例程用的就是bootloader+application的方式开发。我自己这个板子是weact studio的stm32h750vbt6，板载一颗w25q64jv型号的flash，由于h750的片上flash只有128kB，所以我也有了使用bootloader和application的想法。中间歧路没少走，如今成功了，也写一篇文章给大家避避坑。

### 二、 程序编写

bootloader和application是两个独立的工程，不同点在于app位于外部flash而bootloader在内部flash。bootloader主要负责对QSPI器件的初始化、升级用户app等等。application主要实现工程的最终目的。

* bootloader

  1. 初始化QSPI

  	这一步的主要工作是初始化QSPI并使其进入内存映射模式。内存映射后，外部flash的地址为QSPI_BASE == 0x90000000，MCU就可通过总线访问外部flash。值得注意的是，QSPI还有一个QSPI_R_BASE，这是寄存器地址，而QSPI_BASE是外部flash地址。

  	初始化的工作主要包括：

  	* 退出四字节模式：通过向w25qxx发送0xFF命令退出。这个指令在w25q64jv的手册上找不到，包括下面进入四字节模式的0x38也找不到，但是却能用，很奇怪。

  		~~~C++
  		/**
  		 * @brief	exit quad mode, set member QSPI_mode to SPI(false)
  		 * @param	none
  		 */
  		void Flash_T::m_exit_quad_mode(void)
  		{
  			QSPI_CommandTypeDef cmd = {0};
  			cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
  		    cmd.Instruction = 0xFF;
  		    HAL_QSPI_Command(&hqspi, &cmd, 100);
  			m_QSPI_mode = SPI;
  		}
  		~~~

  	* 复位：先发送0x66使能复位，等待busy，再发送0x99复位，等待busy。

  		~~~C++
  		/**
  		 * @brief	reset the w25q64 controller 
  		 * @param	none
  		 */
  		void Flash_T::m_reset(void)
  		{
  			QSPI_CommandTypeDef cmd = {0};
  			
  			cmd.Instruction = 0x66;
  		    cmd.AddressSize = QSPI_ADDRESS_24_BITS;
  			if(m_QSPI_mode)
  				cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
  			else
  				cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  			
  			if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
  				while(1);
  			
  			m_wait(); //wait for busy
  			cmd.Instruction = 0x99;
  			if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
  				while(1);
  		}
  		~~~

  	* 进入四字节模式：写状态寄存器2的QE位；发送0x38进入四字节模式；发送0xC0(Set Read Parameter，不知道有啥用，参考别人的代码时看到了，就写了下来)。

  		注意：进入或退出QSPI模式后要设置一个标志位，之后发送命令或传输数据、地址等用几条线都要根据标志位设置。

  		~~~C++
  		/**
  		 * @brief	enter quad spi mode and set member QSPI_mode to QSPI(true)
  		 * @param	none
  		 *  
  		 */
  		void Flash_T::m_set_quad_mode(void)
  		{
  			uint8_t tmp = 0;
  			m_read_register(&tmp, 2);
  			if((tmp & 0x2) == 0)
  			{
  				tmp |= 0x02;
  				m_write_register(tmp, 2);
  			}
  			if(((tmp >> 1) & 0x1) != 1)
  				while(1);
  		
  			//enter quad mode
  			QSPI_CommandTypeDef cmd = {0};
  			cmd.Instruction = 0x38;
  			cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  		    cmd.AddressSize = QSPI_ADDRESS_24_BITS;
  			if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
  				while(1);
  			m_QSPI_mode = QSPI;
  		
  			//set ReadParam
  			cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
  		    cmd.Instruction = 0xC0;
  		    cmd.DataMode = QSPI_DATA_4_LINES;
  		    cmd.NbData = 1;
  			tmp = 0x03 << 4;
  			m_write_enable();
  			if(HAL_QSPI_Command(&hqspi, &cmd, 100) == HAL_OK)
  		    {
  		        HAL_QSPI_Transmit(&hqspi, &tmp, 100);
  		    }
  		}
  		~~~

  	* 读取ID（可选）：发送0x90（二字节id）或0x9F（三字节id）读取。

  		~~~C++
  		/**
  		 * @brief	read w25q64 id
  		 * @param	none
  		 * @retval	id if read success and HAL_ERROR if sending message error
  		 * @note	you can add something to detect whether what you have read is the correct id 
  		 */
  		uint16_t Flash_T::m_readJEDECID(void)
  		{
  			QSPI_CommandTypeDef cmd = {0};
  			uint8_t tmp[2] = {0};
  			uint16_t ret = 0;
  			cmd.Instruction = 0x90;
  			if(m_QSPI_mode == QSPI) //choose line mode according to QSPI_mode
  				{cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;cmd.DataMode = QSPI_DATA_4_LINES;}
  			else
  				{cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;cmd.DataMode = QSPI_DATA_1_LINE;}
  			cmd.Address = 0;
  			cmd.AddressMode = QSPI_ADDRESS_4_LINES;
  			cmd.AddressSize = QSPI_ADDRESS_24_BITS;
  			
  			cmd.NbData = 2;
  			if(HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK)
  				return HAL_ERROR;
  			
  			if(HAL_QSPI_Receive(&hqspi, tmp, 100) != HAL_OK)
  				return HAL_ERROR;
  			ret |= tmp[0] << 8;
  			ret |= tmp[1] << 0;
  			return ret;
  		}
  		~~~

  	* 进入内存映射模式

  		~~~C++
  		/**
  		 * @brief	enter memory map mode
  		 * @param	none
  		 * @note	you can choose 0xEB or 0x0B for read command 
  		 */
  		void Flash_T::memory_map(void)
  		{
  			QSPI_CommandTypeDef cmd = {0};
  			QSPI_MemoryMappedTypeDef cfg = {0};
  		
  			cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
  			cmd.Instruction = 0xEB; //quad fast read
  			cmd.AddressMode = QSPI_ADDRESS_4_LINES;
  			cmd.AddressSize = QSPI_ADDRESS_24_BITS;
  			cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  			cmd.DataMode = QSPI_DATA_4_LINES;
  			cmd.DummyCycles = 8;
  		
  			cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
  		  	cfg.TimeOutPeriod = 0;
  		 
  			if (HAL_QSPI_MemoryMapped(&hqspi, &cmd, &cfg) != HAL_OK)
  			{
  				LED_Blink(10); //if memory map error, blink to indicate
  			}
  		}
  		~~~

  2. 跳转前准备

  	* 除能Systick

  		~~~C++
  		SysTick->CTRL = 0;	//disable systick
  		SysTick->LOAD = 0;	//clear loaded value
  		SysTick->VAL = 0;	//clear current systick value
  		~~~

  	* 清除pending的中断，进入特权模式，全局中断除能，DeInit IO口。

  		~~~C++
  		for(uint8_t i = 0; i < 8; i++) //clear all NVIC Enable and Pending registers
  		{
  		    NVIC->ICER[i]=0xFFFFFFFF;
  		    NVIC->ICPR[i]=0xFFFFFFFF;
  		}
  		__set_CONTROL(0); //priviage mode 
  		__disable_irq(); //disable interrupt
  		__set_PRIMASK(1);
  		HAL_GPIO_DeInit(GPIOE, GPIO_PIN_3); /deinit io
  		~~~

  3. 跳转操作

  	* 全局定义

  		~~~C++
  		typedef  void (*pFunction)(void);
  		pFunction JumpToApplication; //void (*JumpToApplication)(void);
  		~~~

  	这里的typedef大家可能看不懂，typedef 其实和 define差不多，但是typedef比define更灵活一些。我来举个例子。

  	>一般我们定义函数指针可以这么定义` void (*myfunc)(void) = xxx;`， 这样我们就直接定义了一个无返回值无参数的函数指针叫做'myfunc'，它可以指向任何无返回值无参数的函数。但是这里的typedef是给了void (*xxx)(void)一个别名，typedef后，pFunction也就是“无返回值无参数函数指针类型”的别名，于是第二句就是声明了一个名为`JumpToApplication`的指向无返回值无参数类型函数的函数指针。
  	>
  	>语句` myfunc();`就可以直接调用这个指针指向的函数，但实际上，执行这一句其实是跳转到这个指针的地址执行指令（函数名就是地址）。所以如果我们在bootloader里给这个指针赋一个application里一个函数的地址，就可以实现从bootloader到application的跳转。

  	* 函数内跳转

  		~~~C++
  		JumpToApplication = (pFunction) (*(__IO uint32_t*)(APPLICATION_ADDRESS + 4)); 
  		__set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
  		JumpToApplication(); //jump
  		~~~

  	这里刚接触bootloader的可能看不懂。这里的APPLICATION_ADDRESS就是0x90000000，也就是app的起始地址。在后面的链接文件中，我们把app的中断向量表放在最前面，也就是起始地址0x90000000。熟悉cortex中断向量表的可能知道，表中第一个地址存放的是sp指针，就是堆栈指针；第二个（也就是0x90000004）才是Reset_Handler，是单片机上电后开始运行的第一个函数，它调用SystenInit函数配置时钟，调用C库的\__main(armcc compiler)或__mainCRTStartup(gcc compiler)来初始化sp指针和bss段或者直接在函数内设置sp指针的值，初始化bss段。

  	总之，执行完最后一句后，程序就跳转到App执行。

* application

	建立一个application工程，需要注意这三点：
	
	* 重定位中断向量表
	* 开启在bootloader中除能的中断
	* 修改链接脚本文件，使程序烧录到外部flash(QSPI_BASE)上。
	
	1. 重定位中断向量表只需在main函数开头加上一句` SCB->VTOR = QSPI_BASE;`。你也可以加在其他任何地方，只要你能保证执行这条语句之前没有任何中断发生。
	
	2. 根据在bootloader中的除能配置，重新使能`__enable_irq(); __set_PRIMASK(0);`
	
		最好再清除一次pending的中断，否则有可能发生app不运行的情况。
	
	3. linker scripts的语法和使用
	
	  在这个工程中，我们主要了解MEMORY指令、LMA和VMA。其他的建议去官方资料详细查看。[Linker scripts (haw-hamburg.de)](https://users.informatik.haw-hamburg.de/~krabat/FH-Labor/gnupro/5_GNUPro_Utilities/c_Using_LD/ldLinker_scripts.html#Concepts)
	
	  MEMORY可以定义一个或多个内存区域，可以指定起始地址、长度和属性（可读可写可执行），这些内存区域的名字不能重复。例如：
	
	  ~~~Linker Scripts
	  MEMORY
	  {
	  FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 128K
	  RAM_D1 (xrw)      : ORIGIN = 0x24000000, LENGTH = 512K
	  }
	  ~~~
	
	  这里我们定义了一个名为FLASH的内存区域，它的属性是可读(r)、可执行(x)，起始地址是0x8000000，长度是128KB。还有一个名为RAM_D1的内存区域，它的属性是可读(r)、可执行(x)、可写(w)，起始地址0x24000000，长度为512KB。
	
	  定义了一个内存区域后，我们可以在输出段后添加` >FLASH`来指定该段要加载到哪个地址。例如：
	
	  ~~~Linker Scripts
	  SECTIONS
	  {
	      .isr_vector :
	      {
	          . = ALIGN(4);
	          KEEP(*(.isr_vector)) /* Startup code */
	          . = ALIGN(4);
	      } >FLASH
	      /* Initialized data sections goes into RAM, load LMA copy after code */
	      .data : 
	      {
	          . = ALIGN(4);
	          _sdata = .;        /* create a global symbol at data start */
	          *(.data)           /* .data sections */
	          *(.data*)          /* .data* sections */
	  
	          . = ALIGN(4);
	          _edata = .;        /* define a global symbol at data end */
	      } >RAM_D1 AT> FLASH
	  }
	  ~~~
	
	  这里把.isr_vector段（中断向量表）加载到了FLASH这块区域。而.data段后面有两个指令，一个是` >RAM_D1`， 另一个是` AT> FLASH`。这里就引申出了LMA和VMA的不同了。
	
	  > virtual address (VMA)：VMA是指程序运行时所用的地址，也是大多数情况下我们使用的地址。我们平时用STM32时，程序都以0x08000000为起始地址，这个0x08000000既是VMA也是LMA，因为在官方的system_stm32h7xx.c文件中，定义了向量表的起始地址是0x08000000；在链接文件中，FLASH的起始地址也是0x08000000。一个是在程序中使用的，是程序运行过程中使用的；另一个是在程序外部，程序并不关心。
	  >
	  > <img src="Doc\VMA.png" alt="VMA" style="zoom:80%;" />
	  >
	  > 这里的FLASH_BANK1_BASE也就是0x08000000。
	  >
	  > load address (LMA)：LMA就是我们要烧录程序的地址。对于bootloader，应该下载到0x08000000这个地址，而Application要下载到bootloader之后，在我们这个工程里是0x90000000(QSPI_BASE)。
	  >
	  > bootloader烧录地址：
	  >
	  > <img src="Doc\烧录地址.png" style="zoom: 67%;" />
	  >
	  > 可以看到由于程序需要烧录在0x08000000，所以openocd擦除了0x08000000到0x08005de0的位置。（0x5de0是这个程序的大小）。
	  >
	  > application烧录地址：
	  >
	  > <img src="Doc\烧录地址2.png" style="zoom:67%;" />
	
	  通常.text（代码）和.rodata段都放在FLASH（也就是ROM）里。
	
	  > 关于ROM和RAM，你可能会有些疑惑：如今的FLASH都是可读写的，为何在单片机里还是ROM呢？这是因为我们指定了这块FLASH的属性(attribute)为rx，r为read，x为execute，即可读可执行（仅nor flash支持可执行，nand flash不支持）。而RAM的属性一般是rxw。
	
	4. 修改代码的链接地址
	
	  打开链接脚本文件STM32H750VBTx_FLASH.ld，如果你是keil开发，请移步其他教程（大概是在Target选项卡里修改存储的配置）。把原来FLASH的起始地址008000000改为0x90000000，LENGTH改为8192k。
	
	  <img src="Doc\链接脚本文件修改.png" alt="修改" style="zoom:100%;" />
	
	5. 点灯
	
	  <img src="Doc\点灯.png" alt="点灯" style="zoom:100%;" />
	

### 三、程序烧录

* Openocd配置文件

	打开Openocd安装路径下的\scripts\target文件夹，找到stm32h7x.cfg，复制为stm32h7x_extern.cfg，打开并添加` set QUADSPI 1`。

	<img src="Doc\Openocd配置.png" alt="Openocd" style="zoom:100%;" />

* 编辑makefile

	在makefile最后添加如下语句：

	~~~makefile
	connect:
		openocd -f I:/MCU/Openocd/INSTALL/scripts/interface/cmsis-dap.cfg -f I:/MCU/Openocd/INSTALL/scripts/target/stm32h7x_extern.cfg
	download:
		openocd \
		-f I:/MCU/Openocd/INSTALL/scripts/interface/cmsis-dap.cfg \
		-f I:/MCU/Openocd/INSTALL/scripts/target/stm32h7x_extern.cfg \
		-c init \
		-c halt \
		-c "flash write_image erase $(BUILD_DIR)/$(TARGET).hex 0x00000000" \
		-c reset \
		-c shutdown 
	gdb:
		arm-none-eabi-gdb $(BUILD_DIR)/$(TARGET).elf
	~~~

	注意__connect__ 和__download__ 下的路径要改为自己的路径。

	<img src="Doc\makefile.png" alt="makefile" style="zoom: 80%;" />

	两个工程都配置完成后，`make`编译，再`make download`烧录。__connect__和__gdb__用于gdb调试。烧录完两个程序后，可以看到板子执行了APP的闪灯程序。

	Openocd具体使用请参照上一篇文章或者Openocd官网。

### 四、跳转失败问题总结

1. 时钟配置

	* 在一些不使用外部Flash的bootloader程序里，可能会使用`HAL_RCC_DeInit();`来复位RCC，但是在我们这个工程里不能使用。由于QSPI在整个程序中不能被影响，改变时钟会影响QSPI对Flash的控制，会导致总线无法正常访问外部flash的区域。

	* QSPI时钟源选择。应该选择HCLK为时钟源，因为跳转到App后复位，时钟源会重置为默认的HCLK。

		<img src="Doc\QSPI默认时钟配置.png" style="zoom:100%;" />

2. 内存区域配置

	* 有一些程序会有` SCB_DisableDCache();`或者`SCB_DisableICache();`这两条。如果你的工程使用的是DTCMRAM，并且开启了DCache和ICache，那么就要加上这两条；否则加上会让程序hard fault。

	* 如果你在bootloader中有对内存的读取或者是其他对w25qxx controller做通信的地方，请把这些语句放到memory-map之前。

	* 如果你在创建CubeMX工程时选择了MPU使能，在跳转准备之前加上` HAL_MPU_Disable();`。

	* 在你烧录完bootloader后，确保Openocd能读出w25qxx芯片的信息（具体参考这位大佬的视频[[Linux开发STM32h750\]使用OpenOCD下载程序到外部flash_哔哩哔哩_bilibili](https://www.bilibili.com/video/BV1uS4y1C7bn?spm_id_from=333.880.my_history.page.click&vd_source=d01618f4fdc9bb0fa04158228c78a2ac)），并且在`JumpToApplication();`之前，JumpToApplication是app中Reset_Handler的地址，APPLICATION_ADDRESS指向的是app中堆栈指针的值。

		<img src="Doc\bootloader1.png" style="zoom:100%;" />

		图中0x9000339d是app中Reset_Handler的地址；0x2407ffff是我设置的堆栈指针(_estack)的值。

		Reset_Handler

		<img src="Doc\Reset_Handler.png" style="zoom:100%;" />

		_estack:

		<img src="Doc\_estack.png" style="zoom:100%;" />

		

### 五、结语

这个想法搞到现在四个月，由于中间忙着期末忙着比赛，所以才搞了这么久。实现了自己的想法还是挺开心的。如果文中有问题，希望大家批评指正。

### 附件

[Budali11/stm32h7-bootloader-application: 在stm32h750上使用bootloader+Application开发 (github.com)](https://github.com/Budali11/stm32h7-bootloader-application)

### Reference

an4852-programming-an-external-flash-memory-using-the-uart-bootloader-builtin-stm32-microcontrollers-stmicroelectronics

W25Q64JV_Reference_Manual

安富莱STM32-V7开发板用户手册
