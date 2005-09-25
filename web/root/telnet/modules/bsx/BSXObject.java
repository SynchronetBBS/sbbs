/*
 * BSXObject	-- a simple BSX Object
 * --
 * $Id$
 */
package modules.bsx;

public class BSXObject
{
  public int position = 0, layer = 0;
  public boolean visible = true;

  public BSXObject(int p, int l) { position = p; layer = l; }
}
