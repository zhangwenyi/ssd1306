/*
    MIT License

    Copyright (c) 2018, Alexey Dynda

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "oled_ssd1351.h"
#include "lcd_common.h"
#include "ssd1351_commands.h"
#include "intf/ssd1306_interface.h"
#include "spi/ssd1306_spi.h"
#include "hal/io.h"
#include "nano_gfx_types.h"

#define CMD_ARG     0xFF

extern uint16_t ssd1306_color;
extern uint32_t s_ssd1306_spi_clock;

static const PROGMEM uint8_t s_oled128x128_initData[] =
{
    SSD1351_UNLOCK, CMD_ARG, 0x12,
    SSD1351_UNLOCK, CMD_ARG, 0xB1,
    SSD1351_SLEEP_ON,
    SSD1351_CLOCKDIV, CMD_ARG, 0xF1,         // 7:4 = Oscillator Frequency, 3:0 = CLK Div Ratio (A[3:0]+1 = 1..16)
    SSD1351_SETMULTIPLEX, CMD_ARG, 127,      // Reset to default MUX. See datasheet
    SSD1351_SEGREMAP, CMD_ARG, 0B00110101,   // 16-bit rgb color mode
    SSD1351_SETSTARTLINE, CMD_ARG, 0x00,     // First line to start scanning from
    SSD1351_SETDISPLAYOFFSET, CMD_ARG, 0x00, // Set display offset
    SSD1351_SETGPIO, CMD_ARG, 0x00,          // GPIO OFF
    SSD1351_SETFUNCTION, CMD_ARG, 0x01,
    SSD1351_SETPRECHARGE, CMD_ARG, 0x32,     // Phase 1 and Phase 2 periods
    SSD1351_VCOMH, CMD_ARG, 0x05,            //
    SSD1351_PRECHARGELEVEL, CMD_ARG, 0x17,
    SSD1351_NORMALDISPLAY,
    SSD1351_CONTRAST,  CMD_ARG, 0xC8,        // RED
                       CMD_ARG, 0x80,        // GREEN
                       CMD_ARG, 0xC8,        // BLUE
    SSD1351_MASTERCURRENT, CMD_ARG, 0x0F,    //
    SSD1351_EXTVSL, CMD_ARG, 0xA0, CMD_ARG, 0xB5, CMD_ARG, 0x55,
    SSD1351_PRECHARGESECOND, CMD_ARG, 0x01,  //
    SSD1351_SLEEP_OFF,                    // Disable power-safe mode
    SSD1351_NORMALDISPLAY,
};

static uint8_t s_column;
static uint8_t s_page;

static void ssd1351_setBlock(uint8_t x, uint8_t y, uint8_t w)
{
    uint8_t rx = w ? (x + w - 1) : (s_displayWidth - 1);
    s_column = x;
    s_page = y;
    ssd1306_intf.start();
    ssd1306_spiDataMode(0);
    ssd1306_intf.send(SSD1351_COLUMNADDR);
    ssd1306_spiDataMode(1);  // According to datasheet all args must be passed in data mode
    ssd1306_intf.send(x);
    ssd1306_intf.send(rx < s_displayWidth ? rx : (s_displayWidth - 1));
    ssd1306_spiDataMode(0);
    ssd1306_intf.send(SSD1351_ROWADDR);
    ssd1306_spiDataMode(1);  // According to datasheet all args must be passed in data mode
    ssd1306_intf.send(y<<3);
    ssd1306_intf.send(((y<<3) + 7) < s_displayHeight ? ((y<<3) + 7) : (s_displayHeight - 1));
    ssd1306_spiDataMode(0);
    ssd1306_intf.send(SSD1331_WRITEDATA);
    ssd1306_spiDataMode(1);
}

static void ssd1351_setBlock2(uint8_t x, uint8_t y, uint8_t w)
{
    uint8_t rx = w ? (x + w - 1) : (s_displayWidth - 1);
    ssd1306_intf.start();
    ssd1306_spiDataMode(0);
    ssd1306_intf.send(SSD1351_COLUMNADDR);
    ssd1306_spiDataMode(1);  // According to datasheet all args must be passed in data mode
    ssd1306_intf.send(x);
    ssd1306_intf.send(rx < s_displayWidth ? rx : (s_displayWidth - 1));
    ssd1306_spiDataMode(0);
    ssd1306_intf.send(SSD1351_ROWADDR);
    ssd1306_spiDataMode(1);  // According to datasheet all args must be passed in data mode
    ssd1306_intf.send(y);
    ssd1306_intf.send(s_displayHeight - 1);
    ssd1306_spiDataMode(0);
    ssd1306_intf.send(SSD1331_WRITEDATA);
    ssd1306_spiDataMode(1);
}

static void ssd1351_nextPage(void)
{
    ssd1306_intf.stop();
    ssd1351_setBlock(s_column,s_page+1,0);
}

void    ssd1351_setMode(uint8_t vertical)
{
    ssd1306_intf.start();
    ssd1306_spiDataMode(0);
    ssd1306_intf.send( SSD1351_SEGREMAP );
    ssd1306_spiDataMode(1);
    ssd1306_intf.send( 0B00110100 | vertical );
    ssd1306_intf.stop();
    if (vertical)
    {
        ssd1306_setRamBlock = ssd1351_setBlock;
    }
    else
    {
        ssd1306_setRamBlock = ssd1351_setBlock2;
    }
}

static void ssd1351_sendPixels(uint8_t data)
{
    for (uint8_t i=8; i>0; i--)
    {
        if ( data & 0x01 )
        {
            ssd1306_intf.send( (uint8_t)(ssd1306_color>>8) );
            ssd1306_intf.send( (uint8_t)(ssd1306_color) );
        }
        else
        {
            ssd1306_intf.send( 0B00000000 );
            ssd1306_intf.send( 0B00000000 );
        }
        data >>= 1;
    }
}

static void ssd1351_sendPixelsBuffer(const uint8_t *buffer, uint16_t len)
{
    while(len--)
    {
        ssd1351_sendPixels(*buffer);
        buffer++;
    }
}

static void ssd1351_sendPixel8(uint8_t data)
{
    uint16_t color = RGB8_TO_RGB16(data);
    ssd1306_intf.send( color >> 8 );
    ssd1306_intf.send( color & 0xFF );
}

void    ssd1351_128x128_init()
{
    g_lcd_type = LCD_TYPE_SSD1331;
    s_displayHeight = 128;
    s_displayWidth = 128;
    ssd1306_setRamBlock = ssd1351_setBlock;
    ssd1306_nextRamPage = ssd1351_nextPage;
    ssd1306_sendPixels  = ssd1351_sendPixels;
    ssd1306_sendPixelsBuffer = ssd1351_sendPixelsBuffer;
    ssd1306_sendPixel8 = ssd1351_sendPixel8;
    ssd1306_intf.start();
    ssd1306_spiDataMode(0);
    for( uint8_t i=0; i<sizeof(s_oled128x128_initData); i++)
    {
        uint8_t data = pgm_read_byte(&s_oled128x128_initData[i]);
        if (data == CMD_ARG)
        {
            data = pgm_read_byte(&s_oled128x128_initData[++i]);
            ssd1306_spiDataMode(1);
            ssd1306_intf.send(data);
            ssd1306_spiDataMode(0);
        }
        else
        {
            ssd1306_intf.send(data);
        }
    }
    ssd1306_intf.stop();
}

void   ssd1351_128x128_spi_init(int8_t rstPin, int8_t cesPin, int8_t dcPin)
{
    if (rstPin >=0)
    {
        pinMode(rstPin, OUTPUT);
        digitalWrite(rstPin, HIGH);
        /* Wait at least 1ms after VCC is up for LCD */
        delay(1);
        /* Perform reset operation of LCD display */
        digitalWrite(rstPin, LOW);
        delay(20);
        digitalWrite(rstPin, HIGH);
    }
    /* ssd1351 cannot work faster than at 4MHz per datasheet */
    s_ssd1306_spi_clock = 4000000;
    ssd1306_spiInit(cesPin, dcPin);
    ssd1351_128x128_init();
}
