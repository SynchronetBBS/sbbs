// Replace with char that is dot in center...

/* Small dice
|   |
| ` |

|.  |
|  .|

|.  |
| `.|

|. .|
|. .|

|. .|
|.`.|

-----
|...|
|...|
-----

 .

 :

...

: :

:.:

-----
|:::|
-----
*/

// This function produces dice on the screen...
void SmallDieRoll( INT16 die, INT16 x, INT16 y)
{

    char row1[4], row2[4];
        
        if (od_control.user_rip || od_control.user_ansi)
        {
            // Set the coordinates, so it looks like the dice were thrown...they are randomized
                
            if (die == 1)
            {                          
                sprintf(row1, "   ");
                sprintf(row2, " %c ", 248);
            }
            else if (die == 2)
            {
                sprintf(row1, "%c  ", 9);
                sprintf(row2, "  %c", 9);
            }
            else if (die == 3)
            {
                sprintf(row1, "%c  ", 9);
                sprintf(row2, " %c%c", 248, 9);
            }
            else if (die == 4)
            {
                sprintf(row1, "%c %c", 9,9);
                sprintf(row2, "%c %c", 9,9);
            }
            else if (die == 5)
            {
                sprintf(row1, "%c %c", 9,9);
                sprintf(row2, "%c%c%c", 9,248, 9);
            }
            else if (die == 6)
            {
                sprintf(row1, "%c%c%c", 9,9,9);
                sprintf(row2, "%c%c%c", 9,9,9);
            }
                    
            // Create the boxes that serves as the outside of the dice...
            // od_set_attrib(0x40);
            od_set_attrib(0x04);
            od_draw_box((char) x, (char) y, (char) (x + 4), (char) (y + 3));
//            od_set_attrib(0x4f);
            od_set_cursor((y+1), (x+1));
            od_printf("`bright white on red`%s", row1);
            od_set_cursor((y+2), (x+1));
            od_printf("%s", row2);
        }
        else
        {
            od_printf("`Bright cyan`Die Roll: `Bright white`%d\n\r\n\r", die);
        }

    return;
}

void DeleteDieRoll( INT16 x, INT16 y)
{
                    
   // Create the boxes that serves as the outside of the dice...
   // od_set_attrib(0x40);
   od_set_attrib(0x00);
   od_draw_box((char) x, (char) y, (char) (x + 4), (char) (y + 3));

   return;
}
