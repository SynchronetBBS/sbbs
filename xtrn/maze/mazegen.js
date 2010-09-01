function MazeGenerator(rows,columns) {
	const N=1;			// directional constants
	const E=2;
	const S=4;
	const W=8;

	// generate and display a maze
	this.generate=function(dataFile,mazeFile)
	{
		var mfile=new File(mazeFile);
		var dfile=new File(dataFile);

		var maze=initMaze();	// the maze of cells
		var stack=initStack();	// cell stack to hold a list of cell locations

		generateMaze(maze,stack);

		dfile.open('w');
		dfile.iniSetValue(null,"inprogress",false);
		dfile.iniSetValue(null,"created",system.timer);
		dfile.close();

		mfile.open('wb');
		outputMaze(mfile,maze);
		mfile.close();
	}
	function initMaze()
	{
		var i,j;
		// create a maze of cells
		var maze=new Array(rows);
		for (i=0;i<rows;i++)
			maze[i]=new Array(columns);
		// set all walls of each cell in maze by setting bits :  N E S W
		for (i=0;i<rows;i++)
			for (j=0;j<columns;j++)
				maze[i][j]=(N + E + S + W);
		return maze;
	}
	function initStack()
	{
		// create stack for storing previously visited locations
		var stack=new Array(rows*columns);
		for (i=0;i<rows*columns;i++)
			stack[i]=new Array(2);
		// initialize stack
		for (i=0;i<rows*columns;i++)
			for (j=0;j<2;j++)
				stack[i][j]=parseInt("0");
		return stack;
	}
	function generateMaze(maze,stack)
	{
		var i,j,r,c;
		// choose a cell at random and make it the current cell
		r=random(rows);
		c=random(columns);
		curr=[r,c]; // current search location
		var visited=1;
		var total=rows*columns;
		var tos=0; // index for top of cell stack 
		// arrays of single step movements between cells
		var move=[[-1,0],[0,1],[1,0],[0,-1]];
		var next=[[0,0],[0,0],[0,0],[0,0]];
		while (visited<total) {
			//  find all neighbors of current cell with all walls intact
			j=0;
			for (i=0;i<4;i++) {
				r=curr[0] + move[i][0];
				c=curr[1] + move[i][1];
				//  check for valid next cell
				if((0<=r) && (r<rows) && (0<=c) && (c<columns)) {
					// check ifpreviously visited
					if((maze[r][c] & N) && 
					(maze[r][c] & E) && 
					(maze[r][c] & S) && 
					(maze[r][c] & W)) {
						// not visited,so add to possible next cells
						next[j][0]=r;
						next[j][1]=c;
						j++;
					}
				}
			}
			if(j>0) {
				// current cell has one or more unvisited neighbors,so choose one at random  
				// and knock down the wall between it and the current cell
				i=random(j);
				// next on same row
				if((next[i][0] - curr[0])==0) {
					r=next[i][0];
					// move east
					if(next[i][1]>curr[1]) {
						c=curr[1];
						maze[r][c] &= ~E;          // clear E wall
						c=next[i][1];
						maze[r][c] &= ~W;          // clear W wall
					// move west	
					} else {
						c=curr[1];
						maze[r][c] &= ~W;          // clear W wall
						c=next[i][1];
						maze[r][c] &= ~E;          // clear E wall
					}
				// next on same column
				} else {
					c=next[i][1]
					// move south  
					if(next[i][0]>curr[0]) {
						r=curr[0];
						maze[r][c] &= ~S;          // clear S wall
						r=next[i][0];
						maze[r][c] &= ~N;          // clear N wall
					// move north
					} else {
						r=curr[0];
						maze[r][c] &= ~N;          // clear N wall
						r=next[i][0];
						maze[r][c] &= ~S;          // clear S wall
					}
				}
				tos++; // push current cell location
				stack[tos][0]=curr[0];
				stack[tos][1]=curr[1];
				curr[0]=next[i][0]; // make next cell the current cell
				curr[1]=next[i][1];
				visited++; // increment count of visited cells
			} else	{
				// reached dead end,backtrack
				// pop the most recent cell from the cell stack            
				// and make it the current cell
				curr[0]=stack[tos][0];
				curr[1]=stack[tos][1];
				tos--;
			}
		}
	}
	function outputMaze(file,maze)
	{
		var i,j,k;
		var starting_point=setRandomStart(maze);
		var finish_point=setRandomFinish(maze);
		for(var col=0;col<=(columns*3);col++) output(file," ");
		for (i=0;i<rows;i++) {
			for (k=1;k<=2;k++)	{
				for (j=0;j<columns;j++) {
					if(k==1)      // upper corners and walls (FROM TOP LEFT)
					{
						if(i==0) 
						{
							if(j==0) 
							{
								output(file,"\xDA");		//TOP LEFT CORNER
							}
							else 
							{						//TOP ROW
								if(maze[i][j] & W) //IF WEST WALL IS INTACT IN THE CELL 
								{ 				                        
									output(file,"\xC2");
								}
								else output(file,"\xC4");
							}
							output(file,"\xC4\xC4");
							if((j + 1)==columns) output(file,"\xBF");
						}
						else 
						{
							if(j==0) 
							{						//LEFT EDGE
								if(maze[i][j] & N) output(file,"\xC3\xC4\xC4");
								else output(file,"\xB3  ");
							}
							else 
							{					//MIDDLE OF GRID
								if(maze[i][j] & N) 
								{ 				//IF NORTH WALL IS INTACT IN THIS CELL
									if(maze[i][j-1] & N) 
									{			//AND NORTH WALL IS INTACT IN CELL TO THE LEFT
										if(maze[i][j] & W) 
										{			//AND WEST WALL IS INTACT IN THIS CELL
											if(maze[i-1][j] & W) output(file,"\xC5");	//AND WEST WALL IS INTACT IN CELL ABOVE
											else output(file,"\xC2");					//OTHERWISE DRAW A T
										}
										else 
										{
											if(maze[i-1][j] & W) output(file,"\xC1");//AND WEST WALL IS INTACT IN CELL ABOVE
											else output(file,"\xC4");					//OTHERWISE DRAW A HORIZONTAL LINE
										}
									}
									else 
									{							//IF NORTH WALL IS NOT INTACT IN CELL TO THE LEFT
										if(maze[i][j] & W) 
										{						//AND WEST WALL IS INTACT IN THIS CELL
											if(maze[i-1][j] & W) output(file,"\xC3");//AND WEST WALL IS INTACT IN CELL ABOVE
											else output(file,"\xDA");					//OTHERWISE DRAW A TOP LEFT CORNER
										}	
										else 
										{
											if(maze[i-1][j] & W) output(file,"\xC0");//AND WEST WALL IS INTACT IN CELL ABOVE
											else output(file,"\xC4");					//OTHERWISE DRAW A HORIZONTAL LINE
										}
									}
									output(file, "\xC4\xC4" );
								}
								else 
								{								//IF NORTH WALL IS NOT INTACT IN THIS CELL
			 						if(maze[i][j-1] & N) 
									{							//AND NORTH WALL IS  INTACT IN CELL TO THE LEFT
										if(maze[i][j] & W) 
										{						//AND WEST WALL IS INTACT IN THIS CELL
											if(maze[i-1][j] & W) output(file,"\xB4");	//AND WEST WALL IS INTACT IN CELL ABOVE
											else output(file,"\xBF");					//OTHERWISE DRAW A TOP RIGHT CORNER
										}
										else 
										{						//IF WEST WALL IS NOT INTACT IN THIS CELL
											if(maze[i-1][j] & W) output(file,"\xD9");	//AND WEST WALL IS INTACT IN CELL ABOVE
											else output(file, "\xC4" );				//OTHERWISE DRAW A HORIZONTAL LINE
										}
									}
									else 
									{							//IF NORTH WALL IS NOT INTACT IN CELL TO THE LEFT
										output(file, "\xB3" );
									}
									output(file, "  " );
								}
								if((j + 1)==columns) 
								{
									if(maze[i][j] & N) output(file,"\xB4");
									else output(file,"\xB3");
								}
							}
						}
					}
					else if(k==2)         // center walls and open areas (FROM LEFT)
					{
						if(maze[i][j] & W)  
						{
							output(file,"\xB3");
							if(j==0) 
								if(i==starting_point) output(file,"S ");
								else output(file,"  ");
							else if(j+1==columns) 
								if(i==finish_point) output(file," X");
								else output(file,"  ");
							else output(file,"  ");
						}
						else
							if(j+1==columns && i==finish_point) output(file,"  X");
							else output(file, "   " );
								if((j + 1)==columns) output(file,"\xB3");
					}
				}
			}
		}
		for (j=0;j<columns;j++) 
		{        // bottom walls
			if(j==0) output(file,"\xC0");				//BOTTOM LEFT CORNER
			else if(maze[rows-1][j] & W) output(file,"\xC1");
			else output(file,"\xC4");
			output(file,"\xC4\xC4");
		}
		output(file, "\xD9" );			// BOTTOM RIGHT CORNER
	}
	function setRandomStart(maze)
	{
		for(col=0;col<columns;col++) {
			for(row=0;row<rows;row++) {
				if(maze[row][col+1] & W) {
					if(row+1==rows) return row;
					else if(maze[row][col] & N || maze[row+1][col] & N) return row;
				}
			}
		}
	}
	function setRandomFinish(maze)
	{
		/*
		var randomrow=random(rows);
		write(randomrow)
		return randomrow;
		*/
		for(col=columns;col>0;col--)
		{
			for(row=0;row<rows;row++) {
				if(maze[row][col-1] & W) {
					if(row+1==rows) return row;
					else if(maze[row][col-1] & N || maze[row+1][col-1] & N) return row;
				}
			}
		}
	}
	function output(file,text)
	{
		text=text.replace(/(\S)/g,"$1");
		file.write(text.replace(/ /g," "));
	}
}