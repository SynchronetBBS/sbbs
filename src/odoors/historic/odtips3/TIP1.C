/* Includes */
#include "opendoor.h"

/* Function prototypes. */
void DrawHorizontalBar(int nValue, int nMinValue, int nMaxValue,
   int nMaxChars);
void DrawGraphOfPercentages(int nItems, int *panPercentages,
   char **papszTitles, char bTitleColor, char bGraphColor,
   int nTitleWidth, int nGraphWidth);


/* Main function - program execution begins here. */
main()
{
   char *apszTitles[7] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                          "Thursday", "Friday", "Saturday"};
   int anValues[7] = {50, 75, 100, 25, 83, 0, 43};

   od_printf("`bright green`Test graph:\n\r");

   DrawGraphOfPercentages(7, anValues, apszTitles, 0x02, 0x0f, 20, 50);

   od_get_key(TRUE);

   return(0);
}


/* Function to draw horizontal graph of percentages with titles, to */
/* demonstrate the use of the DrawHorizontalBar() function.         */
/* No titles should have more than nTitleWidth characters.          */
void DrawGraphOfPercentages(int nItems, int *panPercentages,
   char **papszTitles, char bTitleColor, char bGraphColor,
   int nTitleWidth, int nGraphWidth)
{
   int nCurrentItem;

   /* Loop for each item (line) in the graph. */
   for(nCurrentItem = 0; nCurrentItem < nItems; ++nCurrentItem)
   {
      /* Set display color for title text. */
      od_set_attrib(bTitleColor);

      /* Add spaces to right-align all titles. */
      od_repeat(' ', nTitleWidth - strlen(papszTitles[nCurrentItem]));

      /* Display the title. */
      od_disp_str(papszTitles[nCurrentItem]);

      /* Add space between title and graph. */
      od_printf(" ");

      /* Set display color for graph. */
      od_set_attrib(bGraphColor);

      /* Draw bar graph for this line. */
      DrawHorizontalBar(panPercentages[nCurrentItem], 0, 100, nGraphWidth);

      /* Move to the next line. */
      od_printf("\n\r");
   }
}


/* Function to draw a horizontal bar, given a value, the minimum and maximum */
/* possible values, and the number of characters the horizontal bar should   */
/* extended for the maximum value.                                           */
void DrawHorizontalBar(int nValue, int nMinValue, int nMaxValue,
   int nMaxChars)
{
   /* Determine lenght of bar */
   int nBarChars = ((nValue - nMinValue) * nMaxChars) / nMaxValue;

   if(od_control.user_ansi || od_control.user_avatar)
   {
      /* If ANSI or AVATAR graphics are available, assume that IBM extended */
      /* ASCII is also available. This function uses character 220 to form  */
      /* bars that are 1/2 the height of the line. You might also want to   */
      /* try character 119, which will form bars that are the entire height */
      /* of the line.                                                       */
      od_repeat(220, nBarChars);
   }
   else
   {
      /* In ASCII mode, the bar is formed by the '=' character. */
      od_repeat('=', nBarChars);
   }
}
