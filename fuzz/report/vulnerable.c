#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define INPUTSIZE 100

int process(char *input)
{
	char *out;
	char *rest;
	long len; /* hold the full strtol() result so we can range-check it */
	if (strncmp(input, "u ", 2) == 0)
	{ // upper case command
		// how many characters of the string to upper-case
		len = strtol(input + 2, &rest, 10);
		if (rest == input + 2)
		{ // no digits were parsed at all
			return 1;
		}
		if (*rest == ' ')
		{
			rest += 1; // skip the single separating space (only if present)
		}
		// Bytes actually available after the number. The copy and the
		// subsequent strcat() must stay within this many bytes.
		size_t avail = strlen(rest);
		// Reject negative lengths and lengths that run past the input.
		// (strtol overflow also lands here: it saturates to LONG_MAX/MIN.)
		if (len < 0 || (size_t)len > avail)
		{
			printf("Specified length %ld was larger than the input!\n", len);
			return 1;
		}
		// Uppercased prefix (len bytes) + untouched remainder (avail-len) + NUL.
		out = malloc(avail + 1);
		if (out == NULL)
		{
			printf("Failed to allocate memory\n");
			return 1;
		}
		for (long i = 0; i != len; i++)
		{
			char c = rest[i];
			if (c > 96 && c < 123) // ascii a-z
			{
				c -= 32;
			}
			out[i] = c;
		}
		out[len] = 0;
		strcat(out, rest + len); // append the remaining text
		printf("%s", out);
		free(out);
	}
	else if (strncmp(input, "head ", 5) == 0)
	{ // head command
		if (strlen(input) > 6)
		{
			len = strtol(input + 4, &rest, 10);
			if (rest == input + 4)
			{ // no digits were parsed at all
				return 1;
			}
			if (*rest == ' ')
			{
				rest += 1; // skip the single separating space (only if present)
			}
			size_t avail = strlen(rest);
			// rest[len] must land inside the string (0..avail). Reject
			// negative or too-large offsets to avoid an out-of-bounds write.
			if (len < 0 || (size_t)len > avail)
			{
				fprintf(stderr, "head length %ld out of range\n", len);
				return 1;
			}
			rest[len] = '\0'; // truncate string at specified offset
			printf("%s\n", rest);
		}
		else
		{
			fprintf(stderr, "head input was too small\n");
		}
	}
	else if (strcmp(input, "surprise!\n") == 0)
	{
		// Former "easter egg" did `*(char *)1 = 2;`, a wild write that
		// crashes with SIGSEGV. Removed; treat it as an unknown command.
		return 1;
	}
	else
	{
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	char *usage = "Usage: %s\n"
				  "Text utility - accepts commands and data on stdin and prints results to stdout.\n"
				  "\tInput             | Output\n"
				  "\t------------------+-----------------------\n"
				  "\tu <N> <string>    | Uppercased version of the first <N> bytes of <string>.\n"
				  "\thead <N> <string> | The first <N> bytes of <string>.\n";
	char input[INPUTSIZE] = {0};

	// Slurp input. Read at most INPUTSIZE-1 bytes and NUL-terminate so the
	// string functions in process() never read past the buffer, even when
	// the input fills it completely.
	ssize_t n = read(STDIN_FILENO, input, INPUTSIZE - 1);
	if (n < 0)
	{
		fprintf(stderr, "Couldn't read stdin.\n");
		return 1;
	}
	input[n] = '\0';

	int ret = process(input);
	if (ret)
	{
		fprintf(stderr, usage, argv[0]);
	};
	return ret;
}
