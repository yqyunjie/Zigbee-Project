ZigBee Home Automation Sample Applications

These sample applications demonstrate basic ZigBee Home Automation
functionality between a light and a sleepy switch.

The light will perform network operations in response to pressing button 0.  If
the light is not joined to a network, a button press will cause it to perform
an energy scan in order to find an available channel and will then form a
network on that channel.  Once it has formed a network, a button press will
cause the light to permit joining on its network for 60 seconds.  Once on a
network, the light will toggle its LED in response to on/off commands from a
remote.

The switch will sleep until button 0 is pressed.  If the switch is not joined
to a network, a button press will cause the switch to wake up and search for a
joinable network.  If joined, a button press will cause the switch to wake up
and broadcast an on/off command to all nodes in the network.  If the switch has
slept long enough to have aged out of its parent's child table, the switch will
automatically attempt to rejoin the network prior to broadcasting the on/off
command.  Once an action completes, the switch will return to deep sleep.

Both the switch and the light provide feedback during network operations by
playing a tune.  A rising two-tone tune indicates a successful operation while
a falling two-tone tune indicates a failure.  A brief tone indicates that the
device has performed a long-running action and is waiting for a result.

All application code is contained in the _callbacks.c files within each
application directory.

To use each application:

   1. Load the included application configuration file (i.e, the .isc file)
      into Ember Desktop.

   2. Enter a new name for the application in the pop-up window.

   3. Generate the application's header and project/workspace files by
      clicking on the "Generate" button in Ember Desktop.  The application
      files will be generated in the app/builder/<name> directory in the stack
      installation location.

   4. Load the generated project file into the appropriate compiler and build
      the project.

   5. Load the binary image onto a device using Ember Desktop.
