/* This is a cleanup script for Digital Distortion Archive Viewer.
 * This script cleans up temporary files & directories from the
 * node directory used by Digital Distortion Archive Viewer.
 *
 * Author: Eric Oulashin (AKA Nightfox)
 * BBS: Digital Distortion
 * BBS address: digdist.bbsindex.com
 *
 * Date       User              Description
 * 2009-09-03 Eric Oulashin     Created (copied from my logout script).
 *                              Updated deltree() to use the OS's
 *                              recursive delete command because (at
 *                              least in Windows), file_remove() won't
 *                              delete files that have the read-only
 *                              attribute set.
 * 2009-09-04 Eric Oulashin     Updated deltree() to prevent accidentally
 *                              wiping out a root directory.
 * 2009-09-09 Eric Oulashin     Updated deltree() to create a boolean variable
 *                              only once which stores whether or not the
 *                              script is running in Windows.
 * 2009-09-10 Eric Oulashin     Updated deltree() to return true after the
 *                              directory is removed.
 */

load("sbbsdefs.js");

// Remove the Digital Distortion Archive Viewer work directory and
// batch download directory.
deltree(system.node_dir + "DDArcViewerTemp");
deltree(system.node_dir + "DDArcViewer_BatchDL");
// Remove DDArcViewer_DLList.txt - This is the batch download
// queue list file that would be used by the archive viewer if
// possible.
file_remove(system.node_dir + "DDArcViewer_DLList.txt");

// This function recursively removes a directory and all of its contents.  Returns
// whether or not the directory was removed.
//
// Parameters:
//  pDir: The directory to remove (with trailing slash).
//
// Return value: Boolean - Whether or not the directory was removed.
function deltree(pDir)
{
   if ((pDir == null) || (pDir == undefined))
      return false;
   if (typeof(pDir) != "string")
      return false;
   if (pDir.length == 0)
      return false;
   // Make sure pDir actually specifies a directory.
   if (!file_isdir(pDir))
      return false;
   // Don't wipe out a root directory.
   if ((pDir == "/") || (pDir == "\\") || (/:\\$/.test(pDir)) || (/:\/$/.test(pDir)) || (/:$/.test(pDir)))
      return false;

   // If we're on Windows, then use the "RD /S /Q" command to delete
   // the directory.  Otherwise, assume *nix and use "rm -rf" to
   // delete the directory.
   if (deltree.inWindows == undefined)
      deltree.inWindows = (/^WIN/.test(system.platform.toUpperCase()));
   if (deltree.inWindows)
      system.exec("RD " + withoutTrailingSlash(pDir) + " /s /q");
   else
      system.exec("rm -rf " + withoutTrailingSlash(pDir));
   // The directory should be gone, so we should return true.  I'd like to verify that the
   // directory really is gone, but file_exists() seems to return false for directories,
   // even if the directory does exist.  So I test to make sure no files are seen in the dir.
   return (directory(pDir + "*").length == 0);

   /*
   // Recursively deleting each file & dir using JavaScript:
   var retval = true;

   // Open the directory and delete each entry.
   var files = directory(pDir + "*");
   for (var i = 0; i < files.length; ++i)
   {
      // If the entry is a directory, then deltree it (Note: The entry
      // should have a trailing slash).  Otherwise, delete the file.
      // If the directory/file couldn't be removed, then break out
      // of the loop.
      if (file_isdir(files[i]))
      {
         retval = deltree(files[i]);
         if (!retval)
            break;
      }
      else
      {
         retval = file_remove(files[i]);
         if (!retval)
            break;
      }
   }

   // Delete the directory specified by pDir.
   if (retval)
      retval = rmdir(pDir);

   return retval;
   */
}

// Removes a trailing (back)slash from a path.
//
// Parameters:
//  pPath: A directory path
//
// Return value: The path without a trailing (back)slash.
function withoutTrailingSlash(pPath)
{
   if ((pPath == null) || (pPath == undefined))
      return "";

   var retval = pPath;
   if (retval.length > 0)
   {
      var lastIndex = retval.length - 1;
      var lastChar = retval.charAt(lastIndex);
      if ((lastChar == "\\") || (lastChar == "/"))
         retval = retval.substr(0, lastIndex);
   }
   return retval;
}