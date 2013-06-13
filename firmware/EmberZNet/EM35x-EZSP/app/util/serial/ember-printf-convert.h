

#define CONVERT_SLASH_R TRUE

// Transforms Ember printf format specifiers to standard printf specifiers.
// Allocates memory for the new format specifier string which the caller
// must free. 
char* transformEmberPrintfToStandardPrintf(const char* input, 
                                           boolean filterSlashR);
