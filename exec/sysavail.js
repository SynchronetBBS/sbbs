// Control Sysop availability (for chat)
// If called with no arguments, just toggle availability
// otherwise, if argument is "on" make availalble, else: nto available
if(argc)
	system.operator_available = (argv[0].toLowerCase() == "on");
else
	system.operator_available = !system.operator_available;
print("System operator is now: " + (system.operator_available ? "Available" : "Not available"));