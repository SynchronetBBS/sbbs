load("sbbsdefs.js");

var ROWS, COLS;                                 // dimensions of maze
var Maze;                                       // the maze of cells
var Stack;                                      // cell stack to hold a list of cell locations
var N = parseInt("1");                          // direction constants
var E = parseInt("2");
var S = parseInt("4");
var W = parseInt("8");
var maze_file=new File(argv[0]);

compute();

// generate and display a maze
function compute()
{
    ROWS = 10;       // size of maze
    COLS = 26;
//	maze_file.network_byte_order=true;
	maze_file.open('wb');
    init_cells();                               // create and display a maze
    generate_maze();
    writeout_maze();
	maze_file.close();
}

// initialize variable sized multidimensional arrays
function init_cells()
{
    var i, j;

    // create a maze of cells
    Maze = new Array(ROWS);
    for (i = 0; i < ROWS; i++)
	Maze[i] = new Array(COLS);

    // set all walls of each cell in maze by setting bits :  N E S W
    for (i = 0; i < ROWS; i++)
        for (j = 0; j < COLS; j++)
            Maze[i][j] = (N + E + S + W);

    // create stack for storing previously visited locations
    Stack = new Array(ROWS*COLS);
    for (i = 0; i < ROWS*COLS; i++)
        Stack[i] = new Array(2);

    // initialize stack
    for (i = 0; i < ROWS*COLS; i++)
        for (j = 0; j < 2; j++)
            Stack[i][j] = parseInt("0");
}

// use depth first search to create a maze
function generate_maze()
{
    var i, j, r, c;

    // choose a cell at random and make it the current cell
    r = randint( 0, ROWS - 1 );
    c = randint( 0, COLS - 1 );
    curr = [ r, c ];                            // current search location
    var visited  = 1;
    var total = ROWS*COLS;
    var tos   = 0;                              // index for top of cell stack 
    
    // arrays of single step movements between cells
    //          north    east     south    west
    var move = [[-1, 0], [ 0, 1], [ 1, 0], [ 0,-1]];
    var next = [[ 0, 0], [ 0, 0], [ 0, 0], [ 0, 0]];
   
    while (visited < total)
    {
        //  find all neighbors of current cell with all walls intact
        j = 0;
        for (i = 0; i < 4; i++)
        {
            r = curr[0] + move[i][0];
            c = curr[1] + move[i][1];

            //  check for valid next cell
            if ((0 <= r) && (r < ROWS) && (0 <= c) && (c < COLS))
            {
                // check if previously visited
                if ((Maze[r][c] & N) && (Maze[r][c] & E) && (Maze[r][c] & S) && (Maze[r][c] & W))
                {
                    // not visited, so add to possible next cells
                    next[j][0] = r;
                    next[j][1] = c;
                    j++;
                }
            }
        }
        
        if (j > 0)
        {
            // current cell has one or more unvisited neighbors, so choose one at random  
            // and knock down the wall between it and the current cell
            i = randint(0, j-1);

            if ((next[i][0] - curr[0]) == 0)    // next on same row
            {
                r = next[i][0];
                if (next[i][1] > curr[1])       // move east
                {
                    c = curr[1];
                    Maze[r][c] &= ~E;           // clear E wall
                    c = next[i][1];
                    Maze[r][c] &= ~W;           // clear W wall
                }
                else                            // move west
                {
                    c = curr[1];
                    Maze[r][c] &= ~W;           // clear W wall
                    c = next[i][1];
                    Maze[r][c] &= ~E;           // clear E wall
                }
            }
            else                                // next on same column
            {
                c = next[i][1]
                if (next[i][0] > curr[0])       // move south    
                {
                    r = curr[0];
                    Maze[r][c] &= ~S;           // clear S wall
                    r = next[i][0];
                    Maze[r][c] &= ~N;           // clear N wall
                }
                else                            // move north
                {
                    r = curr[0];
                    Maze[r][c] &= ~N;           // clear N wall
                    r = next[i][0];
                    Maze[r][c] &= ~S;           // clear S wall
                }
            }

            tos++;                              // push current cell location
            Stack[tos][0] = curr[0];
            Stack[tos][1] = curr[1];

            curr[0] = next[i][0];               // make next cell the current cell
            curr[1] = next[i][1];

            visited++;                          // increment count of visited cells
        }
        else
        {
            // reached dead end, backtrack
            // pop the most recent cell from the cell stack            
            // and make it the current cell
            curr[0] = Stack[tos][0];
            curr[1] = Stack[tos][1];
            tos--;
        }
    }
}

// draw result in a separate window, call "open_window()" before this
function writeout_maze()
{
    var i, j, k;
	var starting_point=setRandomStart();
	var finish_point=setRandomFinish();
	console.clear();
	for(column=0;column<(COLS*3)+1;column++) output(" ");
	
    for (i = 0; i < ROWS; i++)
    {
        for (k = 1; k <= 2; k++)
        {
            for (j = 0; j < COLS; j++)
            {
                if (k == 1)              // upper corners and walls (FROM TOP LEFT)
                {
					if(i==0) {
						if(j==0) {
							output("\xDA");		//TOP LEFT CORNER
						}
						else {								//TOP ROW
		                    if (Maze[i][j] & W) { 		//IF WEST WALL IS INTACT IN THE CELL 
		                        output( "\xC2" );
							}
							else output("\xC4");
						}
		                output( "\xC4\xC4" );
						if ((j + 1) == COLS) output( "\xBF" );
					}
					else {
						if(j==0) {							//LEFT EDGE
							if (Maze[i][j] & N) output( "\xC3\xC4\xC4" );
							else output( "\xB3  " );
						}
						else {								//MIDDLE OF GRID
		                    if (Maze[i][j] & N) { 				//IF NORTH WALL IS INTACT IN THIS CELL
								if(Maze[i][j-1] & N) {			//AND NORTH WALL IS INTACT IN CELL TO THE LEFT
									if(Maze[i][j] & W) {		//AND WEST WALL IS INTACT IN THIS CELL
										if(Maze[i-1][j] & W) output("\xC5"); //AND WEST WALL IS INTACT IN CELL ABOVE
										else output("\xC2"); 				//OTHERWISE DRAW A T
									}
									else {
										if(Maze[i-1][j] & W) output("\xC1"); //AND WEST WALL IS INTACT IN CELL ABOVE
										else output("\xC4"); 				//OTHERWISE DRAW A HORIZONTAL LINE
									}
								}
								else {							//IF NORTH WALL IS NOT INTACT IN CELL TO THE LEFT
									if(Maze[i][j] & W) {		//AND WEST WALL IS INTACT IN THIS CELL
										if(Maze[i-1][j] & W) output("\xC3"); //AND WEST WALL IS INTACT IN CELL ABOVE
										else output("\xDA"); 				//OTHERWISE DRAW A TOP LEFT CORNER
									}
									else {
										if(Maze[i-1][j] & W) output("\xC0"); //AND WEST WALL IS INTACT IN CELL ABOVE
										else output("\xC4"); 				//OTHERWISE DRAW A HORIZONTAL LINE
									}
								}
		                        output( "\xC4\xC4" );
							}
		                    else {								//IF NORTH WALL IS NOT INTACT IN THIS CELL
		 						if(Maze[i][j-1] & N) {			//AND NORTH WALL IS  INTACT IN CELL TO THE LEFT
									if(Maze[i][j] & W) {		//AND WEST WALL IS INTACT IN THIS CELL
										if(Maze[i-1][j] & W) output("\xB4"); //AND WEST WALL IS INTACT IN CELL ABOVE
										else output("\xBF"); 				//OTHERWISE DRAW A TOP RIGHT CORNER
									}
									else {						//IF WEST WALL IS NOT INTACT IN THIS CELL
										if(Maze[i-1][j] & W) output("\xD9"); //AND WEST WALL IS INTACT IN CELL ABOVE
										else output( "\xC4" );  //OTHERWISE DRAW A HORIZONTAL LINE
									}
								}
								else {							//IF NORTH WALL IS NOT INTACT IN CELL TO THE LEFT
									output( "\xB3" ); 
								}
		                        output( "  " );
							}
		                    if ((j + 1) == COLS) {
								if(Maze[i][j] & N) output("\xB4");
								else output("\xB3");
							}
						}
					}
                }
                else if (k == 2)         // center walls and open areas (FROM LEFT)
                {
					if (Maze[i][j] & W)  {
						output( "\xB3" );
						if(j==0) 
							if(i==starting_point) output("S ");
							else output( "  " );
						else if(j+1==COLS) 
							if(i==finish_point) output(" X");
							else output( "  " );
						else output( "  " );
					}
					else
						if(j+1==COLS && i==finish_point) output("  X");
						else output( "   " );
                    if ((j + 1) == COLS)
                        output( "\xB3" );
                }
            }
        }
    }
	
    for (j = 0; j < COLS; j++) {        // bottom walls
		if(j==0) output("\xC0");				//BOTTOM LEFT CORNER
		else if(Maze[ROWS-1][j] & W) output("\xC1");
		else output("\xC4");
		output("\xC4\xC4");
	}
    output( "\xD9" ); 			// BOTTOM RIGHT CORNER
}
function setRandomStart()
{
	/*
	var randomrow=random(ROWS);
	write(randomrow + " : ")
	return randomrow;
	*/
	for(col=0;col<COLS;col++)
	{
		for(row=0;row<ROWS;row++) {
			if(Maze[row][col+1] & W) {
				if(row+1==ROWS) return row;
				else if(Maze[row][col] & N || Maze[row+1][col] & N) return row;
			}
		}
	}
}
function output(text)
{
	text=text.replace(/(\S)/g,"$1");
	maze_file.write(text.replace(/ /g," "));
}
function setRandomFinish()
{
	/*
	var randomrow=random(ROWS);
	write(randomrow)
	return randomrow;
	*/
	for(col=COLS;col>0;col--)
	{
		for(row=0;row<ROWS;row++) {
			if(Maze[row][col-1] & W) {
				if(row+1==ROWS) return row;
				else if(Maze[row][col-1] & N || Maze[row+1][col-1] & N) return row;
			}
		}
	}
}
function randint( beg, end )
{
    return Math.floor(((end - beg + 1)*Math.random()));
}