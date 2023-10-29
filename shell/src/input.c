#include <unistd.h>
#include <string.h>

#include "input.h"

char buffer[BUF_SIZE];
unsigned int start_pos;
unsigned int end_pos;

void realign()
{
    unsigned int size = end_pos - start_pos;
    if (size <= start_pos) {
        memcpy(buffer, buffer + start_pos, size);
    } else {
        memmove(buffer, buffer + start_pos, size);
    }
    start_pos = 0;
    end_pos = size;
}

/*
 * Read a line from stdin.
 * Return the number of chars read, -1 for errors or 0 for EOF.
 *
 * If the returned value is in [1, MAX_LINE_LENGTH], the pointer to the 0-terminated line
 * is stored in char* at which line_ptr points. Otherwise, its value is undefined.
 */
ssize_t readline(char** line_ptr)
{
    unsigned int cursor_pos = start_pos;
    ssize_t len = 0;

    if (line_ptr == NULL) return -1;

    while (1) {

        /* Attempt to read from buffer */
        while (cursor_pos < end_pos && buffer[cursor_pos] != '\n') ++cursor_pos;
        if (cursor_pos < end_pos) {
            buffer[cursor_pos++] = '\0';
            *line_ptr = buffer + start_pos;
            len += cursor_pos - start_pos;
            start_pos = cursor_pos;
            return len;
        }

        if (len + cursor_pos - start_pos > MAX_LINE_LENGTH) {
            len += cursor_pos - start_pos;
            start_pos = cursor_pos = 0;
        } else if (end_pos + ALIGN_THRESHOLD >= BUF_SIZE) {
            /* Realign data to beginning of buffer if necessary */
            realign();
            cursor_pos = end_pos;
        }

        ssize_t read_count = read(STDIN_FILENO,
                                  buffer + cursor_pos, BUF_SIZE - cursor_pos);
        if (read_count > 0) {
            end_pos = cursor_pos + read_count;
        } else if (read_count == 0) {
            if (len == 0) { // buffer is empty
                return 0;
            } else {
                buffer[cursor_pos++] = '\0'; // this is safe
                *line_ptr = buffer + start_pos;
                len += cursor_pos - start_pos;
                start_pos = cursor_pos;
                return len;
            }
        } else {
            return -1;
        }

    }

}