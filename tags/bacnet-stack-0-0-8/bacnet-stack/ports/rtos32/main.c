/**************************************************************************
*
* Copyright (C) 2005 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/

// This is one way to use the embedded BACnet stack under RTOS-32 
// compiled with Borland C++ 5.02
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "config.h"
#include "bacdef.h"
#include "npdu.h"
#include "apdu.h"
#include "device.h"
#include "handlers.h"
#ifdef BACDL_ETHERNET
  #include "ethernet.h"
  #define bacdl_receive ethernet_receive
  #include "net.h"
  #ifndef HOST
    #include "netcfg.h"
  #endif
#endif
#ifdef BACDL_BIP
  #include "bip.h"
  #define bacdl_receive bip_receive
  #include "net.h"
  #ifndef HOST
    #include "netcfg.h"
  #endif
#endif
#ifdef BACDL_MSTP
  #include "mstp.h"
  #include "rs485.h"
  #define bacdl_receive bip_receive
#endif

#if (defined(BACDL_ETHERNET) || defined(BACDL_BIP))
static int interface = SOCKET_ERROR;  // SOCKET_ERROR means no open interface
#endif

// buffers used for transmit and receive
static uint8_t Rx_Buf[MAX_MPDU] = {0};
#ifdef BACDL_MSTP
volatile struct mstp_port_struct_t MSTP_Port; // port data
static uint8_t MSTP_MAC_Address = 0x05; // local MAC address
#endif

static void Init_Service_Handlers(void)
{
  // we need to handle who-is to support dynamic device binding
  apdu_set_unconfirmed_handler(
    SERVICE_UNCONFIRMED_WHO_IS,
    WhoIsHandler);
  // set the handler for all the services we don't implement
  // It is required to send the proper reject message...
  apdu_set_unrecognized_service_handler_handler(
    UnrecognizedServiceHandler);
  // we must implement read property - it's required!
  apdu_set_confirmed_handler(
    SERVICE_CONFIRMED_READ_PROPERTY,
    ReadPropertyHandler);
  apdu_set_confirmed_handler(
    SERVICE_CONFIRMED_WRITE_PROPERTY,
    WritePropertyHandler);
}

#if (defined(BACDL_ETHERNET) || defined(BACDL_BIP))
/*-----------------------------------*/
static void Error(const char * Msg)
{
   int Code = WSAGetLastError();
#ifdef HOST
   printf("%s, error code: %i\n", Msg, Code);
#else
   printf("%s, error code: %s\n", Msg, xn_geterror_string(Code));
#endif
   exit(1);
}

#ifndef HOST
/*-----------------------------------*/
void InterfaceCleanup(void)
{
   if (interface != SOCKET_ERROR)
   {
      xn_interface_close(interface);
      interface = SOCKET_ERROR;
   #if DEVICE_ID == PRISM_PCMCIA_DEVICE
      RTPCShutDown();
   #endif
   }
}
#endif

static void NetInitialize(void)
// initialize the TCP/IP stack
{
   int Result;

#ifndef HOST

   RTKernelInit(0);                   // get the kernel going

   if (!RTKDebugVersion())            // switch of all diagnostics and error messages of RTIP-32
      xn_callbacks()->cb_wr_screen_string_fnc = NULL;

   CLKSetTimerIntVal(10*1000);        // 10 millisecond tick
   RTKDelay(1);
   RTCMOSSetSystemTime();             // get the right time-of-day

#ifdef RTUSB_VER
   RTURegisterCallback(USBAX172);     // ax172 and ax772 drivers
   RTURegisterCallback(USBAX772);
   RTURegisterCallback(USBKeyboard);  // support USB keyboards
   FindUSBControllers();              // install USB host controllers
   Sleep(2000);                       // give the USB stack time to enumerate devices
#endif

#ifdef DHCP
   XN_REGISTER_DHCP_CLI()             // and optionally the DHCP client
#endif

   Result = xn_rtip_init();           // Initialize the RTIP stack
   if (Result != 0)
      Error("xn_rtip_init failed");

   atexit(InterfaceCleanup);                          // make sure the driver is shut down properly
   RTCallDebugger(RT_DBG_CALLRESET, (DWORD)exit, 0);  // even if we get restarted by the debugger

   Result = BIND_DRIVER(MINOR_0);     // tell RTIP what Ethernet driver we want (see netcfg.h)
   if (Result != 0)
      Error("driver initialization failed");

#if DEVICE_ID == PRISM_PCMCIA_DEVICE
   // if this is a PCMCIA device, start the PCMCIA driver
   if (RTPCInit(-1, 0, 2, NULL) == 0)
      Error("No PCMCIA controller found");
#endif

   // Open the interface
   interface = xn_interface_open_config(DEVICE_ID, MINOR_0, ED_IO_ADD, ED_IRQ, ED_MEM_ADD);
   if (interface == SOCKET_ERROR)
      Error("xn_interface_open_config failed");
   else
   {
      struct _iface_info ii;
      #ifdef BACDL_ETHERNET
      BACNET_ADDRESS my_address;
      unsigned i;
      #endif
      xn_interface_info(interface, &ii);
      printf("Interface opened, MAC address: %02x-%02x-%02x-%02x-%02x-%02x\n",
         ii.my_ethernet_address[0], ii.my_ethernet_address[1], ii.my_ethernet_address[2],
         ii.my_ethernet_address[3], ii.my_ethernet_address[4], ii.my_ethernet_address[5]);
      #ifdef BACDL_ETHERNET
      for (i = 0; i < 6; i++)
      {
         my_address.mac[i] = ii.my_ethernet_address[i];
      }
      ethernet_set_my_address(&my_address);
      #endif
   }

#if DEVICE_ID == PRISM_PCMCIA_DEVICE || DEVICE_ID == PRISM_DEVICE
   xn_wlan_setup(interface,          // iface_no: value returned by xn_interface_open_config()
                 "network name",     // SSID    : network name set in the access point
                 "station name",     // Name    : name of this node
                 0,                  // Channel : 0 for access points, 1..14 for ad-hoc
                 0,                  // KeyIndex: 0 .. 3
                 "12345",            // WEP Key : key to use (5 or 13 bytes)
                 0);                 // Flags   : see manual and Wlanapi.h for details
   Sleep(1000); // wireless devices need a little time before they can be used
#endif // WLAN device

#if defined(AUTO_IP)  // use xn_autoip() to get an IP address
   Result = xn_autoip(interface, MinIP, MaxIP, NetMask, TargetIP);
   if (Result == SOCKET_ERROR)
      Error("xn_autoip failed");
   else
   {
      printf("Auto-assigned IP address %i.%i.%i.%i\n", TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);
      // define default gateway and DNS server
      xn_rt_add(RT_DEFAULT, ip_ffaddr, DefaultGateway, 1, interface, RT_INF);
      xn_set_server_list((DWORD*)DNSServer, 1);
   }
#elif defined(DHCP)   // use DHCP
   {
      DHCP_param   param[] = {{SUBNET_MASK, 1}, {DNS_OP, 1}, {ROUTER_OPTION, 1}};
      DHCP_session DS;
      DHCP_conf    DC;

      xn_init_dhcp_conf(&DC);                 // load default DHCP options
      DC.plist = param;                       // add MASK, DNS, and gateway options
      DC.plist_entries = sizeof(param) / sizeof(param[0]);
      printf("Contacting DHCP server, please wait...\n");
      Result = xn_dhcp(interface, &DS, &DC);  // contact DHCP server
      if (Result == SOCKET_ERROR)
         Error("xn_dhcp failed");
      memcpy(TargetIP, DS.client_ip, 4);
      printf("My IP address is: %i.%i.%i.%i\n", TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);
   }
#else
   // Set the IP address and interface
   printf("Using static IP address %i.%i.%i.%i\n", TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);
   Result = xn_set_ip(interface, TargetIP, NetMask);
   // define default gateway and DNS server
   xn_rt_add(RT_DEFAULT, ip_ffaddr, DefaultGateway, 1, interface, RT_INF);
   xn_set_server_list((DWORD*)DNSServer, 1);
#endif

#else  // HOST defined, run on Windows

   WSADATA wd;
   Result = WSAStartup(0x0101, &wd);

#endif

   if (Result != 0)
      Error("TCP/IP stack initialization failed");
}
#endif

int main(int argc, char *argv[])
{
  BACNET_ADDRESS src = {0};  // address where message came from
  uint16_t pdu_len = 0;
  unsigned timeout = 100; // milliseconds

  (void)argc;
  (void)argv;
  Device_Set_Object_Instance_Number(126);
  Init_Service_Handlers();
  // init the physical layer
  #ifdef BACDL_BIP
  NetInitialize();
 
  bip_set_address(TargetIP[0], TargetIP[1], TargetIP[2], TargetIP[3]);

  // FIXME:
  #if 0
  bip_set_address(NetMask[0], NetMask[1], NetMask[2], NetMask[3]);
extern unsigned long bip_get_addr(void);
    unsigned long broadcast_address = 0;
    unsigned long subnet_mask = 0;

    /* local broadcast address */
    broadcast_address = bip_get_addr();
    broadcast_address |= ~(BIP_Subnet_Mask.s_addr);
    /* configure standard BACnet/IP port */
    bip_set_port(0xBAC0);
  #endif
  if (!bip_init())
    return 1;
  #endif
  #ifdef BACDL_ETHERNET
  NetInitialize();
  if (!ethernet_init(NULL))
    return 1;
  #endif
  #ifdef BACDL_MSTP
  RS485_Initialize();
  MSTP_Init(&MSTP_Port,MSTP_MAC_Address);
  #endif

  
  // loop forever
  for (;;)
  {
    // input
    #ifdef BACDL_MSTP
    MSTP_Millisecond_Timer(&MSTP_Port);
    // note: also called by RS-485 Receive ISR
    RS485_Check_UART_Data(&MSTP_Port);
    MSTP_Receive_Frame_FSM(&MSTP_Port);
    #endif

    #if (defined(BACDL_ETHERNET) || defined(BACDL_BIP))
    // returns 0 bytes on timeout
    pdu_len = bacdl_receive(
      &src,
      &Rx_Buf[0],
      MAX_MPDU,
      timeout);
    #endif

    // process

    if (pdu_len)
    {
      npdu_handler(
        &src,
        &Rx_Buf[0],
        pdu_len);
    }
    if (I_Am_Request)
    {
      I_Am_Request = false;
      Send_IAm();
    }
    // output
    #ifdef BACDL_MSTP
    MSTP_Master_Node_FSM(&MSTP_Port);
    #endif

    // blink LEDs, Turn on or off outputs, etc
  }
}