/**
 * Stocks and other useful utilities for Wintox
 * 
 * Copyright 2012 Zach "theY4Kman" Kanzler
 */

#if defined _wintox_stocks_included
 #endinput
#endif
#define _wintox_stocks_included

#define IS_STR_EMPTY(%1) ((%1)[0] == '\0')
#define STR(%1) (%1), sizeof((%1))

/**
 * Converts the whole String to lower case.
 * Only works with alphabetical characters (not ÖÄÜ) because Sourcemod suxx !
 * The Output String can be the same as the Input String.
 *
 * Taken shamelessly from smlib (http://www.sourcemodplugins.org/pages/smlib/)
 *
 * @param input				Input String.
 * @param output			Output String.
 * @param size				Max Size of the Output string
 * @noreturn
 */
stock String_ToLower(const String:input[], String:output[], size)
{
	size--;

	new x=0;
	while (input[x] != '\0' || x < size) {

		if (IsCharUpper(input[x])) {
			output[x] = CharToLower(input[x]);
		}
		else {
			output[x] = input[x];
		}

		x++;
	}

	output[x] = '\0';
}

// For debugging stacks
stock PrintStack(Handle:stack)
{
    new Handle:temp = CreateStack();
    decl data;
    new length = 0;
    
    while (PopStackCell(stack, data) && ++length)
        PushStackCell(temp, data);
    
    for (new i=0; PopStackCell(temp, data); i++)
    {
        PrintToServer("%3d %d", i, data);
        PushStackCell(stack, data);
    }
}
