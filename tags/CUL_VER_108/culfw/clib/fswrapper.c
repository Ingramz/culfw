#include "fswrapper.h"
#include "display.h"
#include "cdc.h"
#include <stdlib.h>
#include "menu.h"
#include <string.h>

#define BUFSIZE 128
fs_t fs;
static fs_size_t filesize, offset;
static fs_inode_t inode;
static void (*oldinfunc)(void);
static void write_filedata(void);
static uint8_t isMenu;                  // Menu hack

//////////////////////////////////
// Input: Filename 
// Return on error: X+errno(hex)+newline
// Return if ok: length (32bit, hex),newline+data
// If filename is . then return all filenames separated with space + newline
void        
read_file(char *in)
{
  if(in[1] == '.' && in[2] == 0) {              // List directory
    char buf[FS_FILENAME+1];
    uint8_t i = 0;
    while(fs_list(&fs, 0, buf, i) == FS_OK) {
      DS(buf);
      DC('/');
      inode = fs_get_inode( &fs, buf );
      filesize = fs_size(&fs, inode);
      DH((uint16_t)((filesize>>16) & 0xffff),4);
      DH((uint16_t)(filesize & 0xffff),4);
      DC(' ');
      i++;
    }
    DNL();
    return;
  }

  inode = fs_get_inode( &fs, in+1 );
  if(inode == 0xffff) {         // Not found
    DC('X');
    DNL();
    return;

  } else {
    filesize = fs_size(&fs, inode);
    DH((uint16_t)((filesize>>16) & 0xffff),4);
    DH((uint16_t)(filesize & 0xffff),4);
    DNL();

    uint8_t buf[64];
    offset = 0;
    while(offset < filesize) {
      uint8_t len = ((filesize-offset) > sizeof(buf) ? sizeof(buf) : filesize-offset);
      fs_read( &fs, inode, buf, offset, len );
      for(int i = 0; i < len; i++)
        DC(buf[i]);
      offset += sizeof(buf);
    }
  }
}

//////////////////////////////////
// Input: length (32bit, hex) + space + filename + newline
// If length is ffffffff then delete file
// Return on error: X+errno(hex)+newline
// Return if ok: length received (32 bit, hex),newline
void
write_file(char *in)
{
  uint8_t hb[4];
  fs_status_t ret = 8;

  if(fromhex(in+1, hb, 4) != 4)
    goto DONE;

  filesize = ((uint32_t)hb[0]<<24)|((uint32_t)hb[1]<<16)|((uint16_t)hb[2]<<8)|(hb[3]);

  ret = fs_remove(&fs, in+9);
  if(filesize == 0xffffffff) {                            // Delete only
    fs_sync(&fs);
    goto DONE;
  }

  ret = fs_create( &fs, in+9);
  if(ret != FS_OK || filesize == 0)                       // Create only
    goto DONE;

  isMenu = !strcmp(in+9, "MENU");

  inode = fs_get_inode( &fs, in+9 );
  offset = 0;
  oldinfunc = usbinfunc;
  usbinfunc = write_filedata;
  rb_reset(USB_Rx_Buffer);

DONE:
  if(ret == FS_OK) {
    DH((uint16_t)((filesize>>16) & 0xffff),4);
    DH((uint16_t)(filesize & 0xffff),4);
  } else {
    DC('X');
    DH(ret, 2);
  }
  DNL();

  return;
}

void
write_filedata(void)
{
  uint8_t len = USB_Rx_Buffer->nbytes;

  fs_write(&fs, inode, USB_Rx_Buffer->buf, offset, len);
  if(offset+len == filesize) { // Ready
    usbinfunc = oldinfunc;
    fs_sync(&fs);
    if(isMenu) {
      menu_init();
      menu_push(0);
    }
  } else {
    offset += len;
  }
  rb_reset(USB_Rx_Buffer);
}
