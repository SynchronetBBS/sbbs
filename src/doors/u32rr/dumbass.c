#include "files.h"
#include "todo.h"

int main(int argc, char **argv)
{
	open_files();
	player=players;
	player->healing=90;
	return(0);
}
