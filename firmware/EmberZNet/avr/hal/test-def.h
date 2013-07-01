

// Automatic Prototypes
// This block pulls in the file #define as CMDHEADER and is responsible for
// generating the function prototypes for all functions defined in CMD().
#define CMDLIST(listName)
#define CMD(func,cmdName,args,helpStr)  void func(int16u param1, int16u param2);
#define CMD2(func,cmdName,args,helpStr)
#define CMDX(x, func,cmdName,args,helpStr)
#include CMDHEADER
#undef CMDX
#undef CMD2
#undef CMD
#undef CMDLIST


// Flash strings
// This block pulls in the file #define as CMDHEADER and is responsible for
// generating the set of character arrays needed to store the arguement
// strings and the help strings.
#define CMDLIST(listName)
#define CMD(func,cmdName,args,helpStr)  char PGM func##Args[] = args;  \
                                      char PGM func##Help[] = helpStr;
#define CMD2(func,cmdName,args,helpStr) char PGM func##Args2[] = args; \
                                      char PGM func##Help2[] = helpStr;
#define CMDX(x, func,cmdName,args,helpStr) char PGM func##x##Args[] = args; \
                                      char PGM func##x##Help[] = helpStr;

#include CMDHEADER
#undef CMDX
#undef CMD2
#undef CMD
#undef CMDLIST


// Actual definition of the list
// This block pulls in the file #define as CMDHEADER and is responsible for
// generating the data structure, of type commandType and named by the
// CMDLIST macro, that holds all command information.  The commandType is
// defined in haltest.h and this structure is used by the runCommand() and
// printCommands() functions.
#define CMDLIST(listName)               commandType PGM listName[] = {
#define CMD(func,cmdName,args,helpStr)  {func, cmdName, func##Args, func##Help},
#define CMD2(func,cmdName,args,helpStr) {func, cmdName, func##Args2, func##Help2},
#define CMDX(x, func,cmdName,args,helpStr) {func, cmdName, func##x##Args, func##x##Help},
#include CMDHEADER
  {NULL,NULL,NULL,NULL}
};

#undef CMDX
#undef CMD2
#undef CMD
#undef CMDLIST

