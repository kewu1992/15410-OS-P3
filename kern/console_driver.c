/** @file console_driver.c
 *  @brief Console device driver interface and helper function
 *
 *  This file contains functions related to console device driver, including 
 *  interface, helper function and macro.
 *
 *
 *  @author Ke Wu <kewu@andrew.cmu.edu>
 *  @bug No known bugs.
 */

#include <asm.h>
#include <video_defines.h>
#include <spinlock.h>

/** @brief Convert (two dimensional) logical poistion to (one dimensional) 
 *         physical postion */
#define L2P(row,col)    ((row) * CONSOLE_WIDTH + (col))
/** @brief Calculate logical position row given physical position */
#define P2R(offset)     ((offset) / CONSOLE_WIDTH)
/** @brief Calculate logical position column given physical position */
#define P2C(offset)     ((offset) % CONSOLE_WIDTH)
/** @brief Check if the color is valid */
#define CHECK_COLOR(color)  (!((color) & 0xFFFFFF00))
/** @brief Check if the logical position row is valid */
#define CHECK_ROW(row)      (((row) >= 0) && ((row) < CONSOLE_HEIGHT))
/** @brief Check if the logical position column is valid */
#define CHECK_COL(col)      (((col) >= 0) && ((col) < CONSOLE_WIDTH))

/** @brief Stores which color the console is currently using, it can be 
 *         accessed by set_term_color() and get_term_color() */
static char term_color;

/** @brief Becuase hardware cursor might be hidden (set beyond the console), 
 *         a logical cursor is necessary for driver to know where the next 
 *         character will be placed on the screen. it can be accessed by 
 *         set_cursor() and get_cursor() 
 */
static uint16_t logical_cursor;

/** @brief To indicate if the hardware cursor is currently hidden */
static char is_hidden;

static spinlock_t spinlock;

/** @brief Initialize console device driver
 *  
 *  This function will be called by handler_install() to initialize console 
 *  device driver. It get the default color and initial curor poistion of 
 *  console and also set hardware is visible by default. 
 *
 *  @return Void
 */
void init_console_driver() {
    // get initial default color
    term_color = *(char *)(CONSOLE_MEM_BASE + 1);
    
    // get initial cursor location
    outb(CRTC_IDX_REG, CRTC_CURSOR_LSB_IDX);
    uint8_t value_lo = inb(CRTC_DATA_REG);
    outb(CRTC_IDX_REG, CRTC_CURSOR_MSB_IDX);
    uint8_t value_hi = inb(CRTC_DATA_REG);
    logical_cursor = ((uint16_t)value_hi << 8) | (uint16_t)value_lo;

    is_hidden = 0;

    spinlock_init(&spinlock);
}

/** @brief Set hardware cursor through I/O port
 *  
 *  Tell CRTC the postion of hardware cursor through I/O port
 *
 *  @param offset The desired position of hardware cursor 
 *  @return Void
 */
void set_hardware_cursor(uint16_t offset) {
    uint8_t value_lo = (uint8_t)offset;
    uint8_t value_hi = (uint8_t)(offset >> 8);
    outb(CRTC_IDX_REG, CRTC_CURSOR_LSB_IDX);
    outb(CRTC_DATA_REG, value_lo);
    outb(CRTC_IDX_REG, CRTC_CURSOR_MSB_IDX);
    outb(CRTC_DATA_REG, value_hi);
}

/** @brief Scroll up the screen one line
 *  
 *  Copy data from line i to line i-1, and put spaces on the last line. 
 *
 *  @return Void
 */
void scrollup() {
    int end_index = (CONSOLE_HEIGHT - 1) * CONSOLE_WIDTH * 2;
    int offset = CONSOLE_WIDTH * 2;
    int i;
    // copy data from line i to line i-1
    for (i = 0; i < end_index; i += 2){
        *(char *)(CONSOLE_MEM_BASE + i) = 
                                    *(char *)(CONSOLE_MEM_BASE + i + offset);
        *(char *)(CONSOLE_MEM_BASE + i + 1) = 
                                *(char *)(CONSOLE_MEM_BASE + i + 1 + offset);
    }
    end_index += CONSOLE_WIDTH*2;
    // put spaces on the last line
    for (; i < end_index; i += 2){
        *(char *)(CONSOLE_MEM_BASE + i) = ' ';
        *(char *)(CONSOLE_MEM_BASE + i + 1) = term_color;
    }
}

/** @brief Prints character ch at the current location
 *         of the cursor.
 *
 *  If the character is a newline ('\n'), the cursor is moved
 *  to the beginning of the next line (scrolling if necessary).
 *  If the character is a carriage return ('\r'), the cursor is
 *  immediately reset to the beginning of the current line,
 *  causing any future output to overwrite any existing output
 *  on the line.  If backspace ('\b') is encountered, the previous
 *  character is erased.  See the main console.c description found
 *  on the handout web page for more backspace behavior.
 *
 *  This function is not thread-safe, it is caller's reponsibility
 *  to make sure that no one putbyte() is interleaved with another
 *
 *  @param ch the character to print
 *  @return The input character
 */
int putbyte(char ch)
{
    uint16_t offset = logical_cursor;

    switch(ch) {
    case '\n':
        if (P2R(offset) == CONSOLE_HEIGHT - 1){
            scrollup();
            logical_cursor = L2P(P2R(offset),0);
        } else {
            logical_cursor = L2P(P2R(offset)+1,0);
        }
        break;
    case '\r':
        logical_cursor = L2P(P2R(offset),0);
        break;
    case '\b':
        // can backspace only if it is not at the beginning of a line
        if (P2C(offset) > 0) { 
            offset--;
            logical_cursor = offset;
            *(char *)(CONSOLE_MEM_BASE + 2 * offset) = ' ';
            *(char *)(CONSOLE_MEM_BASE + 2 * offset + 1) = term_color;
        }
        break;
    default:
        *(char *)(CONSOLE_MEM_BASE + 2 * offset) = ch;
        *(char *)(CONSOLE_MEM_BASE + 2 * offset + 1) = term_color;

        if (offset == CONSOLE_HEIGHT * CONSOLE_WIDTH - 1) {
            scrollup();
            logical_cursor = L2P(P2R(offset),0);
        }
        else
            logical_cursor = offset + 1;
    }

    if (!is_hidden)
        set_hardware_cursor(logical_cursor);
    
    return ch; 
}

/** @brief Prints the string s, starting at the current
 *         location of the cursor.
 *
 *  If the string is longer than the current line, the
 *  string fills up the current line and then
 *  continues on the next line. If the string exceeds
 *  available space on the entire console, the screen
 *  scrolls up one line, and then the string
 *  continues on the new line.  If '\n', '\r', and '\b' are
 *  encountered within the string, they are handled
 *  as per putbyte. If len is not a positive integer or s
 *  is null, the function has no effect.
 *
 *  @param s The string to be printed.
 *  @param len The length of the string s.
 *  @return Void.
 */
void putbytes(const char *s, int len) {
    if (!s)
        return;
    int i;
    for (i = 0; i < len; i++) {
        spinlock_lock(&spinlock);
        putbyte(s[i]);
        spinlock_unlock(&spinlock);
    }
}

/** @brief Prints character ch with the specified color
 *         at position (row, col).
 *
 *  If any argument is invalid, the function has no effect.
 *
 *  @param row The row in which to display the character.
 *  @param col The column in which to display the character.
 *  @param ch The character to display.
 *  @param color The color to use to display the character.
 *  @return Void.
 */
void draw_char(int row, int col, int ch, int color) {
    if (!(CHECK_ROW(row) && CHECK_COL(col) && CHECK_COLOR(color)))
        return;
    int offset = row * CONSOLE_WIDTH + col;
    *(char *)(CONSOLE_MEM_BASE + 2 * offset) = ch;
    *(char *)(CONSOLE_MEM_BASE + 2 * offset + 1) = (char)(color & 0xFF);
}

/** @brief Returns the character displayed at position (row, col).
 *  @param row Row of the character.
 *  @param col Column of the character.
 *  @return The character at (row, col).
 */
char get_char(int row, int col) {
    if (!(CHECK_ROW(row) && CHECK_COL(col)))
        return 0;
    return *(char *)(CONSOLE_MEM_BASE + 2 * (row * CONSOLE_WIDTH + col));
}

/** @brief Changes the foreground and background color
 *         of future characters printed on the console.
 *
 *  If the color code is invalid, the function has no effect.
 *
 *  @param color The new color code.
 *  @return 0 on success or integer error code less than 0 if
 *          color code is invalid.
 */
int set_term_color(int color) {
    if (!CHECK_COLOR(color))
        return -1;
    term_color = color;
    return 0;
}

/** @brief Writes the current foreground and background
 *         color of characters printed on the console
 *         into the argument color.
 *  @param color The address to which the current color
 *         information will be written.
 *  @return Void.
 */
void get_term_color(int *color) {
    *color = term_color;
}

/** @brief Sets the position of the cursor to the
 *         position (row, col).
 *
 *  Subsequent calls to putbytes should cause the console
 *  output to begin at the new position. If the cursor is
 *  currently hidden, a call to set_cursor() does not show
 *  the cursor.
 *
 *  @param row The new row for the cursor.
 *  @param col The new column for the cursor.
 *  @return 0 on success or integer error code less than 0 if
 *          cursor location is invalid.
 */
int set_cursor(int row, int col) {
    if (!(CHECK_ROW(row) && CHECK_COL(col)))
        return -1;
    logical_cursor = L2P(row, col);
    if (!is_hidden)
        set_hardware_cursor(logical_cursor);
    return 0;
}

/** @brief Writes the current position of the cursor
 *         into the arguments row and col.
 *  @param row The address to which the current cursor
 *         row will be written.
 *  @param col The address to which the current cursor
 *         column will be written.
 *  @return Void.
 */
void get_cursor(int *row, int *col) {
    *row = P2R(logical_cursor);
    *col = P2C(logical_cursor);
}

/** @brief Hides the cursor.
 *
 *  Subsequent calls to putbytes do not cause the
 *  cursor to show again.
 *
 *  @return Void.
 */
void hide_cursor() {
    is_hidden = 1;
    set_hardware_cursor(CONSOLE_WIDTH * CONSOLE_WIDTH);
}

/** @brief Shows the cursor.
 *  
 *  If the cursor is already shown, the function has no effect.
 *
 *  @return Void.
 */
void show_cursor() {
    is_hidden = 0;
    set_hardware_cursor(logical_cursor);
}

/** @brief Clears the entire console.
 *
 *  The cursor is reset to the first row and column
 *
 *  @return Void.
 */
void clear_console() {
    int end_index = CONSOLE_HEIGHT * CONSOLE_WIDTH * 2;
    int i;
    for (i = 0; i < end_index; i += 2) {
        *(char *)(CONSOLE_MEM_BASE + i) = ' ';
        *(char *)(CONSOLE_MEM_BASE + i + 1) = term_color;
    }
    
    logical_cursor = 0;
    if (!is_hidden)
        set_hardware_cursor(0);
}

