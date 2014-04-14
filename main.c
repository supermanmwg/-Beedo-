#include "stdio.h"
#include "ctype.h"
#include "stm32f2xx.h"
#include "platform.h"
#include "mxchipWNET.h"
#include "menu.h"
#include "stm32f2xx_gpio.h"

#define DynamicMemAlloc          
#define WEB_SERVER			"t.upan.pro"
#define APP_INFO          	"mxchipWNet Demo: WPS and Easylink demo"

//for GPIO
GPIO_InitTypeDef  GPIO_InitStructure;

network_InitTypeDef_st 	wNetConfig;
lib_config_t 			libConfig;
int 					wifi_up = 0;
int 					serverConnectted = 0;
int 					sslConnectted = 0;
int 					https_server_addr = 0;
int 					dns_pending = 0;
extern u32 				MS_TIMER;
int 					configSuccess = 0;
int 					easylink = 0;
int 					menu_enable = 1;
int						check_responce=0;

const char sendhttpRequest[]="GET /api.php?type=send&v=1&id=1 HTTP/1.1\r\n\
Accept: text/html, application/xhtml+xml, */*\r\n\
Accept-Language: en\r\n\
User-Agent: Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; WOW64; Trident/6.0)\r\n\
Host: t.upan.pro\r\n\
Connection: Keep-Alive\r\n\r\n";

const char responcehttpRequest[]="GET /api.php?type=status&id=1 HTTP/1.1\r\n\
Accept: text/html, application/xhtml+xml, */*\r\n\
Accept-Language: en\r\n\
User-Agent: Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; WOW64; Trident/6.0)\r\n\
Host: t.upan.pro\r\n\
Connection: Keep-Alive\r\n\r\n";

const char menu[] =
   "\n"
   "+***************(C) COPYRIGHT 2013 MXCHIP corporation************+\n"
   "|          EMW316x WPS and EasyLink configuration demo           |\n"
   "+ command ----------------+ function ----_-----------------------+\n"
   "| 1:WPS                   | WiFi Protected Setup                 |\n"
   "| 2:EasyLink              | One step configuration from MXCHIP   |\n"
   "| 3:EasyLink_V2           | One step configuration from MXCHIP   |\n"
   "| 4:REBOOT                | Reboot                               |\n"
   "| ?:HELP                  | displays this help                   |\n"
   "+-------------------------+--------------------------------------+\n"
   "|                           By William Xu from MXCHIP M2M Team   |\n"
   "+----------------------------------------------------------------+\n";

void print_msg(void);

void config_gpio_pb8()
{
	// GPIOB Periph clock enable 
 	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

 	// Configure PB8 in output pushpull mode 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void config_gpio_pc6() 
{
 	// GPIOC Periph clock enable 
  	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

  	// Configure PC2 in output pushpull mode 
  	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

// ========================================
//	User provide callback functions
// ======================================== 
void system_version(char *str, int len)
{
  snprintf( str, len, "%s", APP_INFO);
}    

void RptConfigmodeRslt(network_InitTypeDef_st *nwkpara)
{
	if(nwkpara == NULL)
	{
		printf("Configuration failed\r\n");
		printf ("\nMXCHIP> ");
		menu_enable = 1;
	}
	else
	{
		configSuccess = 1;
		memcpy(&wNetConfig, nwkpara, sizeof(network_InitTypeDef_st));
		printf("Configuration is successful, SSID:%s, Key:%s\r\n", \
				wNetConfig.wifi_ssid,\
				wNetConfig.wifi_key);
	}
}
		
void easylink_user_data_result(int datalen, char* data)
{
   net_para_st para;
   getNetPara(&para, Station);
   if(!datalen)
   {
    printf("No user input. %s\r\n", data);
   }
   else
   {
    printf("User input is %s\r\n", data);
   }
}    
    
void userWatchDog(void)
{
}

void WifiStatusHandler(int event)
{
  switch (event) 
  {
    case MXCHIP_WIFI_UP:
						printf("Station up \r\n");
						printf ("\nMXCHIP> ");
						menu_enable = 1;
						//easylink = 1;      //what is its meaning?
						break;
    case MXCHIP_WIFI_DOWN:
						printf("Station down \r\n");
						break;
    default:
			break;
  }
  return;
}

void ApListCallback(UwtPara_str *pApList)
{

}

void NetCallback(net_para_st *pnet)
{
	printf("IP address: %s \r\n", pnet->ip);
	printf("NetMask address: %s \r\n", pnet->mask);
	printf("Gateway address: %s \r\n", pnet->gate);
	printf("DNS server address: %s \r\n", pnet->dns);
	printf("MAC address: %s \r\n", pnet->mac);
}

void dns_ip_set(u8 *hostname, u32 ip)
{
	char ipstr[16];
	https_server_addr = ip;
    dns_pending = 0;
	if(serverConnectted == -1)
		printf("DNS test: %s failed \r\n", WEB_SERVER);
	else
	{
		inet_ntoa(ipstr, ip);
		printf("1DNS test: %s address is %s \r\n", WEB_SERVER, ipstr);
	}
	printf("1http server addr=%x\n",https_server_addr);
}

void socket_connected(int fd)
{
	serverConnectted = 1;
}

void sysTick_configuration(void)
{
  // Setup SysTick Timer for 10 msec interrupts  
  if (SysTick_Config(SystemCoreClock / 100)) //SysTick≈‰÷√∫Ø ˝
  { 
    // Capture error  
    while (1);
  }  
}

void mwgtimer(void)
{
	static __IO uint32_t TimingDelayLocal = 0;
  
  if (TimingDelayLocal <= 200)
  {
    TimingDelayLocal++; 
  }
  else
  {
  	check_responce=1;
    TimingDelayLocal = 0;
  }
}

int r_http(char *buf)
{
        char *httpbuffer;
       
        int data_len=0;
        int len_num=0;
        int wr_sign=0;
        int i=0;
        int con=216;
        char signchar[5]="\r\n\r\n";
		 httpbuffer=buf;
        for(i=0;i<con;i++)
                if(strncmp(&httpbuffer[i],signchar,4)==0)
                        break;

        len_num=i+4;
        while(httpbuffer[len_num]!='\r'&&httpbuffer[len_num+1]!='\n')
        {
                if(httpbuffer[len_num]>='0'&&httpbuffer[len_num]<='9')
                        data_len=data_len*10+httpbuffer[len_num++]-'0';
                else if(httpbuffer[len_num]>='a'&&httpbuffer[len_num]<='f')
                        data_len=data_len*10+httpbuffer[len_num++]-'a'+10;
                else
                {
                        printf("error data length!\n");
                        return 0;
                }
        }
        printf("data length is %d\n",data_len);

        wr_sign=httpbuffer[len_num+2]-'0';

        printf("read value is %d data %c\n",wr_sign,httpbuffer[len_num+2]);

        return wr_sign;

}

int main(void)
{
	int 			fd_client = -1;
	char 			*buf;
	int 			con = -1;
	int 			opt = 0;
	fd_set 			readfds, exceptfds;
	struct 			timeval_t t;
	struct 			sockaddr_t addr;
	lib_config_t 	libConfig;
	volatile int	setval;
	volatile int	readval;

	buf = (char*)malloc(3*1024);

	libConfig.tcp_buf_dynamic = mxEnable;
	libConfig.tcp_max_connection_num = 12;
	libConfig.tcp_rx_size = 2048;
	libConfig.tcp_tx_size = 2048;
	libConfig.hw_watchdog = 0;
	libConfig.wifi_channel = WIFI_CHANNEL_1_13;
	lib_config(&libConfig);
  
	mxchipInit();
	UART_Init();


	printf("\r\n%s\r\n mxchipWNet library version: %s\r\n", APP_INFO, system_lib_version());
	printf(menu);
	printf ("\nMXCHIP> ");
	
	//init http
	set_tcp_keepalive(3, 60);
	setSslMaxlen(6*1024);
	t.tv_sec = 0;
  	t.tv_usec = 100;
	
//	void GPIO_SetBits(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);

	while(1)
	{
		mxchipTick();	
		if(menu_enable)
			Main_Menu();
		if(configSuccess)
		{
			wNetConfig.wifi_mode = Station;
			wNetConfig.dhcpMode = DHCP_Client;
			StartNetwork(&wNetConfig);
			printf("connect to %s.....\r\n", wNetConfig.wifi_ssid);
			configSuccess = 0;
			menu_enable = 1;

			sysTick_configuration();
			config_gpio_pc6();
			config_gpio_pb8();

			while(1) 
			{
				readval= GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_6);
				mxchipTick();
				//If wifi is established, connect to WEBSERVER, and send a http request
				if(https_server_addr == 0 && dns_pending == 0)
				{  
					//DNS function
					https_server_addr = dns_request(WEB_SERVER);
					if(https_server_addr == -1)
						printf("DNS test: %s failed. \r\n", WEB_SERVER); 
					else if (https_server_addr == 0) //DNS pending, waiting for callback
					{
						dns_pending = 1;
						printf("2DNS test: %s success. \r\n", WEB_SERVER); 
						printf("21http server addr=%x\n",https_server_addr);
					}
				}
				
				if( fd_client == -1 && (u32)https_server_addr>0)
				{
					fd_client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
					setsockopt(fd_client,0,SO_BLOCKMODE,&opt,4);
					addr.s_ip = https_server_addr; 
					addr.s_port = 443;
					if (connect(fd_client, &addr, sizeof(addr))!=0) 
						printf("Connect to %s failed.\r\n", WEB_SERVER); 
					else   
						printf("Connect to %s success.\r\n", WEB_SERVER); 	
				}

				if(serverConnectted&&sslConnectted==0)
				{
					printf("Connect to web server success! Setup SSL ecryption...\r\n");
					if (setSSLmode(1, fd_client)!= MXCHIP_SUCCESS)
					{
						printf("SSL connect fail\r\n");
						close(fd_client);
						fd_client = -1;
						serverConnectted = 0;
					} 
					else 
					{
						printf("SSL connect\r\n");
						sslConnectted = 1;
					}
				}	
		
				if(readval==1&&sslConnectted)
				{
					printf("read value is 1\n");
					send(fd_client, sendhttpRequest, strlen(sendhttpRequest), 0);
				//	return 0;
				}
				if(check_responce&&sslConnectted)
				{
					printf("readval=%d\n",readval);
					check_responce=0;
					send(fd_client, responcehttpRequest, strlen(responcehttpRequest), 0);
					//Check status on erery sockets 
					FD_ZERO(&readfds);

					if(sslConnectted)
						FD_SET(fd_client, &readfds);
		
					select(1, &readfds, NULL, &exceptfds, &t);
		
					//Read html data from WEBSERVER
					if(sslConnectted)
					{
						if(FD_ISSET(fd_client, &readfds))
						{
							con = recv(fd_client, buf, 2*1024, 0);
							if(con > 0)
							{
								printf("Get %s data successful! data length: %d bytes data\r\n", WEB_SERVER, con);
								setval=r_http(buf);
								printf("read from http  is %d\n",setval);
					
								if(setval==1)
									GPIO_SetBits(GPIOB,GPIO_Pin_8);
								 else
								  	GPIO_ResetBits(GPIOB,GPIO_Pin_8);
							}
							else
							{
								close(fd_client);
								serverConnectted = 0;
								sslConnectted = 0;
								fd_client = -1;
								printf("Web connection closed.\r\n");
							}
						}
					}
				}
			}
		}
		
		if(easylink)
		{
			OpenEasylink2(60);	
			easylink = 0;
		}
	}
}

