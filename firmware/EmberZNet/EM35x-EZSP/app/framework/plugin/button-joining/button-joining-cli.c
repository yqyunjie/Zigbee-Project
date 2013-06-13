// *****************************************************************************
// * button-joining-cli.c
// *
// * CLI commands for forming/joining using the hardware buttons.
// *
// * Copyright 2013 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"

static void optionButton0Command(void);
static void optionButton1Command(void);

extern void emberAfPluginButtonJoiningPressButton(int8u button);

EmberCommandEntry emberAfPluginButtonJoiningCommands[] = {
  {"button0", optionButton0Command, ""},
  {"button1", optionButton1Command, ""},
  { NULL },
};

static void optionButton0Command(void)
{
  emberAfPluginButtonJoiningPressButton(BUTTON0);
}

static void optionButton1Command(void)
{
  emberAfPluginButtonJoiningPressButton(BUTTON1);
}
