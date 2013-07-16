// *******************************************************************
//  lcd.h
//
//  common functions for lcd driver  128*64
//
//  Copyright by kaiser. All rights reserved.
// *******************************************************************
#ifndef _LCD_H_
#define _LCD_H_

/*----------------------------------------------------------------------------
 *        Macro
 *----------------------------------------------------------------------------*/
#define GPIO_PxCLR_BASE (GPIO_PACLR_ADDR)
#define GPIO_PxSET_BASE (GPIO_PASET_ADDR)
#define GPIO_PxOUT_BASE (GPIO_PAOUT_ADDR)
// Each port is offset from the previous port by the same amount
#define GPIO_Px_OFFSET  (GPIO_PBCFGL_ADDR-GPIO_PACFGL_ADDR)

#define LCD_SCK_PIN  PORTA_PIN(2)  	/*接口定义:lcd_sclk 就是 LCD 的 sclk*/
#define LCD_SID_PIN  PORTA_PIN(1)    /*接口定义:lcd_sid 就是 LCD 的 sid*/
#define LCD_RS_PIN   PORTA_PIN(0)    /*接口定义:lcd_rs 就是 LCD 的 rs*/
//#define lcd_reset=P1^0; /*接口定义:lcd_reset 就是 LCD 的 reset*/
#define LCD_CS1_PIN  PORTA_PIN(3)   /*接口定义:lcd_cs1 就是 LCD 的 cs1*/

#define ROM_IN_PIN   PORTA_PIN(0)  /*字库 IC 接口定义:Rom_IN 就是字库 IC 的 SI*/
#define ROM_OUT_PIN  PORTA_PIN(1)   /*字库 IC 接口定义:Rom_OUT 就是字库 IC 的 SO*/
#define ROM_SCK_PIN  PORTA_PIN(2)   /*字库 IC 接口定义:Rom_SCK 就是字库 IC 的 SCK*/
#define ROM_CS_PIN   PORTC_PIN(2)    /*字库 IC 接口定义 Rom_CS 就是字库 IC 的 CS#*/

#define LCD_PIN_OUT_MODE GPIOCFG_OUT

/* LCD_SCK_ACTION **/
#define LCD_SCK(x)        \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(LCD_SCK_PIN/8)))) = BIT(LCD_SCK_PIN & 7); \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(LCD_SCK_PIN/8)))) = BIT(LCD_SCK_PIN & 7);  \
		  }                \
		}while(0)

/* LCD_SID_ACTION **/
#define LCD_SID(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(LCD_SID_PIN/8)))) = BIT(LCD_SID_PIN & 7); \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(LCD_SID_PIN/8)))) = BIT(LCD_SID_PIN & 7); \
		  }                \
		}while(0)

/* LCD_RS_PIN  **/
#define LCD_RS(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(LCD_RS_PIN/8)))) = BIT(LCD_RS_PIN & 7);   \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(LCD_RS_PIN/8)))) = BIT(LCD_RS_PIN & 7);   \
		  }                \
		}while(0)

/* LCD_CS1_PIN  **/
#define LCD_CS1(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(LCD_CS1_PIN/8)))) = BIT(LCD_CS1_PIN & 7);    \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(LCD_CS1_PIN/8)))) = BIT(LCD_CS1_PIN & 7);    \
		  }                \
		}while(0)

/* ROM_IN_PIN  **/
#define ROM_IN(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(ROM_IN_PIN/8)))) = BIT(ROM_IN_PIN & 7);    \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(ROM_IN_PIN/8)))) = BIT(ROM_IN_PIN & 7);     \
		  }                \
		}while(0)

/* ROM_OUT_PIN  **/
#define ROM_OUT(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(ROM_OUT_PIN/8)))) = BIT(ROM_OUT_PIN & 7);   \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(ROM_OUT_PIN/8)))) = BIT(ROM_OUT_PIN & 7);   \
		  }                \
		}while(0)

/* ROM_SCK_PIN  **/
#define ROM_SCK(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(ROM_SCK_PIN/8)))) = BIT(ROM_SCK_PIN & 7);   \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(ROM_SCK_PIN/8)))) = BIT(ROM_SCK_PIN & 7);   \
		  }                \
		}while(0)

/* ROM_CS_PIN  **/
#define ROM_CS(x)         \
		do{                \
		  if(x){           \
		  	*((volatile int32u *)(GPIO_PxSET_BASE+(GPIO_Px_OFFSET*(ROM_CS_PIN/8)))) = BIT(ROM_CS_PIN & 7);    \
		  }                \
		  else{            \
		    *((volatile int32u *)(GPIO_PxCLR_BASE+(GPIO_Px_OFFSET*(ROM_CS_PIN/8)))) = BIT(ROM_CS_PIN & 7);    \
		  }                \
		}while(0)


/*----------------------------------------------------------------------------
 *        Typedef
 *----------------------------------------------------------------------------*/
typedef  unsigned char  uchar;
typedef  unsigned int   uint;
typedef  unsigned long  ulong;


/*----------------------------------------------------------------------------
 *        Global Extern
 *----------------------------------------------------------------------------*/
extern const int8u jiong1[];
extern const int8u lei1[];
extern const int8u bmp1[];

/*----------------------------------------------------------------------------
 *        Prototype
 *----------------------------------------------------------------------------*/
/****************************************************
 * LCD module initial.
***************************************************/
void initial_lcd();

/****************************************************
 * LCD screen clear.
***************************************************/
void clear_screen();

/****************************************************
 * Display image by 128*64 pixel.
***************************************************/
void display_128x64(const int8u *dp);

/****************************************************
 * Display string by 5*7 pixel.
***************************************************/
void display_string_5x7(uchar y,uchar x,uchar *text);

/****************************************************
 * Display by 16*16 pixel.
***************************************************/
void display_graphic_16x16(uint page,uint column, const int8u *dp);

/****************************************************
 * Display string and image by 8*16 pixel
***************************************************/
void display_graphic_8x16(uint page,uchar column,uchar *dp);

/****************************************************
 * Display string and image by 5*7 pixel
***************************************************/
void display_graphic_5x7(uint page,uchar column,uchar *dp);

/****************************************************
 * Display GB2312 string.
***************************************************/
void display_GB2312_string(uchar y,uchar x,uchar *text);


#endif//_LCD_H_
//eof