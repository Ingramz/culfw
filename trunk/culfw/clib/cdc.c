#include "led.h"
#include "ringbuffer.h"
#include "ttydata.h"
#include "cdc.h"

/* Globals: */
CDC_Line_Coding_t LineCoding = { BaudRateBPS: 9600,
                                 CharFormat:  OneStopBit,
                                 ParityType:  Parity_None,
                                 DataBits:    8            };

DEFINE_RBUF(Tx_Buffer, TX_SIZE)

EVENT_HANDLER(USB_ConfigurationChanged)
{
  LED_ON();
  /* Setup CDC Notification, Rx and Tx Endpoints */
  Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPNUM, EP_TYPE_INTERRUPT,
      ENDPOINT_DIR_IN, CDC_NOTIFICATION_EPSIZE, ENDPOINT_BANK_SINGLE);

  Endpoint_ConfigureEndpoint(CDC_TX_EPNUM, EP_TYPE_BULK,
      ENDPOINT_DIR_IN, CDC_TXRX_EPSIZE, ENDPOINT_BANK_DOUBLE);

  Endpoint_ConfigureEndpoint(CDC_RX_EPNUM, EP_TYPE_BULK,
      ENDPOINT_DIR_OUT, CDC_TXRX_EPSIZE, ENDPOINT_BANK_DOUBLE);

  Scheduler_SetTaskMode(CDC_Task, TASK_RUN);	
  LED_OFF();
}

//////////////////////////////////////////////
// Implement the "Modem" Part of the ACM Spec., i.e ignore the requests.
EVENT_HANDLER(USB_UnhandledControlPacket)
{
  uint8_t* LineCodingData = (uint8_t*)&LineCoding;

  /* Process CDC specific control requests */
  switch (bRequest)
  {
    case GET_LINE_CODING:
      if (bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
      {
        /* Acknowedge the SETUP packet, ready for data transfer */
        Endpoint_ClearSetupReceived();

        /* Write the line coding data to the control endpoint */
        Endpoint_Write_Control_Stream_LE(LineCodingData, sizeof(LineCoding));
 
        /* Send the line coding data to the host and clear the control endpoint */
        Endpoint_ClearSetupOUT();
      }
 
      break;
    case SET_LINE_CODING:
      if (bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
      {
        /* Acknowedge the SETUP packet, ready for data transfer */
        Endpoint_ClearSetupReceived();

        /* Read the line coding data in from the host into the global struct */
        Endpoint_Read_Control_Stream_LE(LineCodingData, sizeof(LineCoding));

        /* Send the line coding data to the host and clear the control endpoint */
        Endpoint_ClearSetupIN();
      }
 
      break;
    case SET_CONTROL_LINE_STATE:
      if (bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
      {
        /* Acknowedge the SETUP packet, ready for data transfer */
        Endpoint_ClearSetupReceived();
 
        /* Send an empty packet to acknowedge the command (currently unused) */
        Endpoint_ClearSetupIN();
      }

      break;
  }
}


////////////////////
// Fill data from USB to the RingBuffer and vice-versa
TASK(CDC_Task)
{
  if(!USB_IsConnected)
    return;

  Endpoint_SelectEndpoint(CDC_RX_EPNUM); // Select the Serial Rx Endpoint
  if(Endpoint_ReadWriteAllowed()){       // USB -> RingBuffer

    while (Endpoint_BytesInEndpoint()) { // If the buffer is full, data will
                                         // be discarded
      analyze_ttydata(Endpoint_Read_Byte());
    }
    Endpoint_ClearCurrentBank(); 
  }


  if (Tx_Buffer->nbytes)               // RingBuffer -> USB
  {
    Endpoint_SelectEndpoint(CDC_TX_EPNUM);
    while (!(Endpoint_ReadWriteAllowed()));
    
    bool IsFull = (Endpoint_BytesInEndpoint() == CDC_TXRX_EPSIZE);
    
    cli();
    while (Tx_Buffer->nbytes && (Endpoint_BytesInEndpoint() < CDC_TXRX_EPSIZE))
      Endpoint_Write_Byte(rb_get(Tx_Buffer));
    sei();
    
    Endpoint_ClearCurrentBank();        // Send the data

    /* If a full endpoint was sent, we need to send an empty packet afterwards
     * to terminate the transfer */
    if(IsFull) {

      while (!(Endpoint_ReadWriteAllowed()))
        ;

      Endpoint_ClearCurrentBank();
    }
  }
}
