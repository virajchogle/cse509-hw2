#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define INPUTSIZE 100

int process(char *input)
{
	char *out;
	char *rest;
	long len; // use long so we can range-check the strtol result
	if (strncmp(input, "u ", 2) == 0)
	{ // upper case command
		len = strtol(input + 2, &rest, 10); // how many characters to upper-case
		if (rest == input + 2)
			return 1; // no number given
		if (*rest == ' ')
			rest += 1; // skip the separating space if present
		size_t avail = strlen(rest);
		if (len < 0 || (size_t)len > avail) // reject negative/too-large len
		{
			printf("Specified length %ld was larger than the input!\n", len);
			return 1;
		}
		out = malloc(avail + 1); // enough for the whole remaining string + NUL
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
				return 1; // no number given
			if (*rest == ' ')
				rest += 1; // skip the separating space if present
			size_t avail = strlen(rest);
			if (len < 0 || (size_t)len > avail) // keep rest[len] in bounds
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
		return 1; // was *(char*)1 = 2 (crash); treat as unknown command
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

	// Slurp input
	if (read(STDIN_FILENO, input, INPUTSIZE) < 0)
	{
		fprintf(stderr, "Couldn't read stdin.\n");
	}

	int ret = process(input);
	if (ret)
	{
		fprintf(stderr, usage, argv[0]);
	};
	return ret;
}
