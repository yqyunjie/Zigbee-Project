// *******************************************************************
//  lcd.c
//
//  common functions for lcd driver  128*64
//
//  Copyright by kaiser. All rights reserved.
// *******************************************************************
/*----------------------------------------------------------------------------
 *        Includes
 *----------------------------------------------------------------------------*/
#include "app/sensor/common.h"
#include "app/sensor/lcd.h"

/*----------------------------------------------------------------------------
 *        Global Variable
 *----------------------------------------------------------------------------*/
/*--  文字:  囧  --*/
/*--  宋体 12;  此字体下对应的点阵为：宽 x 高=16x16   --*/
const int8u jiong1[]={
0x00,0xFE,0x82,0x42,0xA2,0x9E,0x8A,0x82,0x86,0x8A,0xB2,0x62,0x02,0xFE,0x00,0x00,
0x00,0x7F,0x40,0x40,0x7F,0x40,0x40,0x40,0x40,0x40,0x7F,0x40,0x40,0x7F,0x00,0x00};

const int8u lei1[]={/*--  文字:  畾  --*/
/*--  宋体 12;  此字体下对应的点阵为：宽 x 高=16x16   --*/
0x80,0x80,0x80,0xBF,0xA5,0xA5,0xA5,0x3F,0xA5,0xA5,0xA5,0xBF,0x80,0x80,0x80,0x00,
0x7F,0x24,0x24,0x3F,0x24,0x24,0x7F,0x00,0x7F,0x24,0x24,0x3F,0x24,0x24,0x7F,0x00};

const int8u bmp1[]={
/*--  调入了一幅图像：D:\我的文档\My Pictures\12864-555.bmp  --*/
/*--  宽度 x 高度=128x64  --*/
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x80,0x80,0xC0,0xC0,0xC0,0xC0,0xE0,0x60,0xE0,0xE0,0xE0,0xE0,0x60,

0x60,0x60,0x60,0x60,0x60,0x60,0xE0,0xE0,0xE0,0xE0,0xC0,0xC0,0xC0,0xC0,0x80,0x80,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

};

const int8u bmp2[]={
/*--  调入了一幅图像：E:\work\图片收藏夹\JLX12864G-1016-1.bmp  --*/
/*--  宽度 x 高度=128x64  --*/
0xFF,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
0x01,0x01,0x01,0x01,0x01,0x01,0x01,0xC1,0xC1,0x41,0x41,0x41,0x41,0x41,0xC1,0xC1,
0x01,0x01,0x01,0x01,0x41,0xC1,0xC1,0x41,0x41,0xC1,0xC1,0x61,0x61,0xC1,0x81,0x01,
0xE1,0xE1,0x41,0x01,0x01,0x01,0x21,0xE1,0xC1,0x81,0x41,0x41,0xC1,0xC1,0x41,0x41,
0x41,0xC1,0xC1,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0xE1,0xE1,0x01,

};

/*----------------------------------------------------------------------------
 *        Functions
 *----------------------------------------------------------------------------*/
/****************************************************
 * Write command into LCD module.
***************************************************/
void transfer_command_lcd(int8u data1)
{
 char i;
 LCD_RS(0); //lcd_rs=0;
 for(i=0;i<8;i++)
 {
   LCD_SCK(0);	//lcd_sclk=0;
   if(data1 &0x80 )
	 LCD_SID(1);	//lcd_sid=1;
   else
   	 LCD_SID(0);	//lcd_sid=0;
   LCD_SCK(1);		//lcd_sclk=1;
   data1 = data1<<1;
 }
}

/****************************************************
 * Write data into LCD module.
***************************************************/
void transfer_data_lcd(int8u data1)
{
 char i;
 LCD_RS(1);	//lcd_rs=1;
 for(i=0;i<8;i++)
 {
   LCD_SCK(0);	//lcd_sclk=0;
   if(data1&0x80)
	 LCD_SID(1);	//lcd_sid=1;
   else
	 LCD_SID(0);	//lcd_sid=0;
   LCD_SCK(1);		//lcd_sclk=1;
   data1 = data1<<1;
 }
}

/****************************************************
 * Delay.
***************************************************/
void delay(int n_ms)
{
 int j,k;
 for(j=0;j<n_ms;j++)
   for(k=0;k<110;k++);
}

/****************************************************
 * LCD module initial.
***************************************************/
void initial_lcd()
{
  /** initial LCD pins configuration. */
  GPIO_PACFGL_REG = (LCD_PIN_OUT_MODE << PA3_CFG_BIT) |
	 				(LCD_PIN_OUT_MODE << PA2_CFG_BIT) |
					(LCD_PIN_OUT_MODE << PA1_CFG_BIT) |
                    (LCD_PIN_OUT_MODE << PA0_CFG_BIT);

  GPIO_PCCFGL_REG |= LCD_PIN_OUT_MODE << PC2_CFG_BIT ;

  	/******************************
    * initial LCD pios.
    ******************************/

   LCD_CS1(0);	//lcd_cs1=0;
   ROM_CS(1); 	//Rom_CS = 1;
   //lcd_reset=0;        /*低电平复位*/
   delay(20);
   //lcd_reset=1;        /*复位完毕*/
   delay(20);
   transfer_command_lcd(0xe2);  /*软复位*/
   delay(5);
   transfer_command_lcd(0x2c);  /*升压步聚 1*/
   delay(5);
   transfer_command_lcd(0x2e);  /*升压步聚 2*/
   delay(5);
   transfer_command_lcd(0x2f);  /*升压步聚 3*/
   delay(5);
   transfer_command_lcd(0x23);  /*粗调对比度，可设置范围 0x20～0x27*/
   transfer_command_lcd(0x81);  /*微调对比度*/
   transfer_command_lcd(0x1f);  /*0x28,微调对比度的值，可设置范围0x00～0x3f*/
   transfer_command_lcd(0xa2);  /*1/9 偏压比（bias）*/
   transfer_command_lcd(0xc8);  /*行扫描顺序：从上到下*/
   transfer_command_lcd(0xa0);  /*列扫描顺序：从左到右*/
   transfer_command_lcd(0x40);  /*起始行：第一行开始*/
   transfer_command_lcd(0xaf);  /*开显示*/
   LCD_CS1(1);	//lcd_cs1=1;
}

/****************************************************
 * LCD address calculation.
***************************************************/
void lcd_address(uint page,uint column)
{

 column=column-0x01;
 transfer_command_lcd(0xb0+page-1);   /*设置页地址*/
 transfer_command_lcd(0x10+(column>>4&0x0f));  /*设置列地址的高 4 位*/
 transfer_command_lcd(column&0x0f);  /*设置列地址的低 4 位*/
}

/****************************************************
 * LCD screen clear.
***************************************************/
void clear_screen()
{
 unsigned char i,j;
 LCD_CS1(0);	//lcd_cs1=0;
 ROM_CS(1);	//Rom_CS = 1;
 for(i=0;i<9;i++)
 {
   transfer_command_lcd(0xb0+i);
   transfer_command_lcd(0x10);
   transfer_command_lcd(0x00);
   for(j=0;j<132;j++)
   {
       transfer_data_lcd(0x00);
   }
 }
 LCD_CS1(1);	//lcd_cs1=1;
}

/****************************************************
 * Display image by 128*64 pixel.
***************************************************/
void display_128x64(const int8u *dp)
{
 uint i,j;
 LCD_CS1(0);	//lcd_cs1=0;
 for(j=0;j<8;j++)
 {
   lcd_address(j+1,1);
   for (i=0;i<128;i++)
   {
     transfer_data_lcd(*dp);         /*写数据到 LCD,每写完一个 8 位的数据后列地址自动加 1*/
     dp++;
   }
 }
 LCD_CS1(1);	//lcd_cs1=1;
}

/****************************************************
 * Display by 16*16 pixel.
***************************************************/
void display_graphic_16x16(uint page,uint column, const int8u *dp)
{
 uint i,j;
 LCD_CS1(0);	//lcd_cs1=0;
 ROM_CS(1);		//Rom_CS = 1;
 for(j=0;j<2;j++)
 {
   lcd_address(page+j,column);
   for (i=0;i<16;i++)
   {
     transfer_data_lcd(*dp);         /*写数据到 LCD,每写完一个 8 位的数据后列地址自动加 1*/
     dp++;
   }
 }
 LCD_CS1(1);	//lcd_cs1=1;
}


/****************************************************
 * Display string and image by 8*16 pixel
***************************************************/
void display_graphic_8x16(uint page,uchar column,uchar *dp)
{
 uint i,j;
 LCD_CS1(0);	//lcd_cs1=0;
 for(j=0;j<2;j++)
 {
   lcd_address(page+j,column);
   for (i=0;i<8;i++)
   {
     transfer_data_lcd(*dp);         /*写数据到 LCD,每写完一个 8 位的数据后列地址自动加 1*/
     dp++;
   }
 }
 LCD_CS1(1);	//lcd_cs1=1;
}


/****************************************************
 * Display string and image by 5*7 pixel
***************************************************/
void display_graphic_5x7(uint page,uchar column,uchar *dp)
{
 uint col_cnt;
 uchar page_address;
 uchar column_address_L,column_address_H;
 page_address = 0xb0+page-1;

 LCD_CS1(0);	//lcd_cs1=0;

 column_address_L =(column&0x0f)-1;
 column_address_H =((column>>4)&0x0f)+0x10;

 transfer_command_lcd(page_address);    /*Set Page Address*/
 transfer_command_lcd(column_address_H);  /*Set MSB of column Address*/
 transfer_command_lcd(column_address_L);  /*Set LSB of column Address*/

 for (col_cnt=0;col_cnt<6;col_cnt++)
 {
   transfer_data_lcd(*dp);
   dp++;
 }
 LCD_CS1(1);	//lcd_cs1=1;
}

/****************************************************
 * Send command into LCD ROM
***************************************************/
void send_command_to_ROM( uchar datu )
{
 uchar i;
 for(i=0;i<8;i++ )
 {
   if(datu&0x80)
     ROM_IN(1);	//Rom_IN = 1;
   else
     ROM_IN(0); //Rom_IN = 0;
   datu = datu<<1;
   ROM_SCK(0);	//Rom_SCK=0;
   ROM_SCK(1);	//Rom_SCK=1;
 }
}

/****************************************************
 * Get one char from ROM.
***************************************************/
static uchar get_data_from_ROM( void )
{
 uchar i;
 uchar ret_data=0;
 ROM_SCK(1);	//Rom_SCK=1;
 for(i=0;i<8;i++)
 {
   ROM_OUT(1);	//Rom_OUT=1;
   ROM_SCK(0);	//Rom_SCK=0;
   ret_data=ret_data<<1;
   //if( Rom_OUT )
   //  ret_data=ret_data+1;
   //else
   //  ret_data=ret_data+0;
   ROM_SCK(1);	//Rom_SCK=1;
 }
 return(ret_data);
}



/****************************************************
 * Get a string data from ROM.
***************************************************/
void get_n_bytes_data_from_ROM(uchar addrHigh,uchar addrMid,uchar addrLow,uchar *pBuff,uchar DataLen )
{
 uchar i;
 ROM_CS(0);	//Rom_CS = 0;
 LCD_CS1(1);	//lcd_cs1=1;
 ROM_SCK(0);	//Rom_SCK=0;
 send_command_to_ROM(0x03);
 send_command_to_ROM(addrHigh);
 send_command_to_ROM(addrMid);
 send_command_to_ROM(addrLow);
 for(i = 0; i < DataLen; i++ )
   *(pBuff+i) =get_data_from_ROM();
 ROM_CS(1);	//Rom_CS = 1;
}


/****************************************************
 * Display GB2312 string.
***************************************************/
ulong  fontaddr=0;
void display_GB2312_string(uchar y,uchar x,uchar *text)
{
 uchar i= 0;
 uchar addrHigh,addrMid,addrLow ;
 uchar fontbuf[32];
 while((text[i]>0x00))
 {
   if(((text[i]>=0xb0) &&(text[i]<=0xf7))&&(text[i+1]>=0xa1))
   {
     /*国标简体（GB2312）汉字在晶联讯字库 IC 中的地址由以下公式来计算：*/
     /*Address = ((MSB - 0xB0) * 94 + (LSB - 0xA1)+ 846)*32+ BaseAdd;BaseAdd=0*/
     /*由于担心 8 位单片机有乘法溢出问题，所以分三部取地址*/
     fontaddr = (text[i]- 0xb0)*94;
     fontaddr += (text[i+1]-0xa1)+846;
     fontaddr = (ulong)(fontaddr*32);

     addrHigh = (fontaddr&0xff0000)>>16;  /*地址的高 8 位,共 24 位*/
     addrMid = (fontaddr&0xff00)>>8;      /*地址的中 8 位,共 24 位*/
     addrLow = fontaddr&0xff;       /*地址的低 8 位,共 24 位*/
     get_n_bytes_data_from_ROM(addrHigh,addrMid,addrLow,fontbuf,32 );/*取 32 个字节的数据，存到"fontbuf[32]"*/
     display_graphic_16x16(y,x,fontbuf);/*显示汉字到 LCD 上，y为页地址，x 为列地址，fontbuf[]为数据*/
     i+=2;
     x+=16;
   }
   else if((text[i]>=0x20) &&(text[i]<=0x7e))
   {
     unsigned char fontbuf[16];
     fontaddr = (text[i]- 0x20);
     fontaddr = (unsigned long)(fontaddr*16);
     fontaddr = (unsigned long)(fontaddr+0x3cf80);
     addrHigh = (fontaddr&0xff0000)>>16;
     addrMid = (fontaddr&0xff00)>>8;
     addrLow = fontaddr&0xff;

     get_n_bytes_data_from_ROM(addrHigh,addrMid,addrLow,fontbuf,16 );/*取 16 个字节的数据，存到"fontbuf[32]"*/

     display_graphic_8x16(y,x,fontbuf);/*显示 8x16 的 ASCII字到 LCD 上，y 为页地址，x 为列地址，fontbuf[]为数据*/
     i+=1;
     x+=8;
   }
   else
     i++;
 }

}

/****************************************************
 * Display string by 5*7 pixel.
***************************************************/
void display_string_5x7(uchar y,uchar x,uchar *text)
{
 unsigned char i= 0;
 unsigned char addrHigh,addrMid,addrLow ;
 while((text[i]>0x00))
 {
   if((text[i]>=0x20) &&(text[i]<=0x7e))
   {
     unsigned char fontbuf[8];
     fontaddr = (text[i]- 0x20);
     fontaddr = (unsigned long)(fontaddr*8);
     fontaddr = (unsigned long)(fontaddr+0x3bfc0);
     addrHigh = (fontaddr&0xff0000)>>16;
     addrMid = (fontaddr&0xff00)>>8;
     addrLow = fontaddr&0xff;

     get_n_bytes_data_from_ROM(addrHigh,addrMid,addrLow,fontbuf,8);/*取 8 个字节的数据，存到"fontbuf[32]"*/

     display_graphic_5x7(y,x,fontbuf);/*显示 5x7 的 ASCII 字到LCD 上，y 为页地址，x 为列地址，fontbuf[]为数据*/
     i+=1;
     x+=6;
   }
   else
     i++;
 }
}

//eof
