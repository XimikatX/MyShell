#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "input.h"

static char buffer[BUF_SIZE];

static char* start = buffer;
static char* end = buffer;

static void realign()
{
    unsigned int size = end - start;

    if (size <= start - buffer) {
        memcpy(buffer, start, size);
    } else {
        memmove(buffer, start, size);
    }
    start = buffer;
    end = buffer + size;
}

/*
 * Read a line_ptr from stdin.
 * Return the number of chars read, -1 for errors or 0 for EOF.
 *
 * If the returned value is in [1, MAX_LINE_LENGTH], the pointer to the 0-terminated line_ptr
 * is stored in char* at which line_ptr points. Otherwise, its value is undefined.
 */
ssize_t readline(char** line_ptr)
{
    char* cursor = start;
    ssize_t len = 0;

    if (line_ptr == NULL) return -1;

    while (1) {

        /* Attempt to read from buffer */
        cursor = memchr(cursor, '\n', end - cursor);
        if (cursor != NULL) {
            *cursor++ = '\0';
            *line_ptr = start;
            len += cursor - start;
            start = cursor;
            return len;
        } else {
            cursor = end;
        }

        if (len + (cursor - start) > MAX_LINE_LENGTH) {
            len += cursor - start;
            start = cursor = buffer;
        } else if (end - buffer + ALIGN_THRESHOLD >= BUF_SIZE) {
            realign();
            cursor = end;
        }

        ssize_t read_count = read(STDIN_FILENO, cursor, BUF_SIZE - (cursor - buffer));
        if (read_count > 0) {
            end = cursor + read_count;
        } else if (read_count == 0) {
            if (len == 0) {
                return 0;
            } else {
                *cursor++ = '\0';
                *line_ptr = start;
                len += cursor - start;
                start = cursor;
                return len;
            }
        } else if (errno != EINTR) {
            return -1;
        }

    }

}