function generateMaze(columns,rows) {
	/* bitwise wall constants */
	const N=1;
	const E=2;
	const S=4;
	const W=8;
	
	function initMaze()
	{
		// create a maze of cells
		var maze=new Array(rows);
		for (var i=0;i<rows;i++) {
			maze[i]=new Array(columns);
			// set all walls of each cell in maze by setting bits :  N E S W
			for (var j=0;j<columns;j++)
				maze[i][j]=(N + E + S + W);
		}
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
				stack[i][j]=0;
		return stack;
	}
	function generateMaze()
	{
		var maze=initMaze();	// the maze of cells
		var stack=initStack();	// cell stack to hold a list of cell locations
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
		return maze;
	}
	
	var maze = generateMaze();
	return maze;
}