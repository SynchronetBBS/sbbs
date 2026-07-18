import "syncterm_menu" for Menu
import "syncterm" for Key
import "menu_ui" for MenuUi
import "ui_popup" for Alert, Confirm

class SortProfiles {
  static field_(code) {
    var wanted = code.abs
    for (row in Menu.sortFields) {
      if (row[0] == wanted) return row
    }
    return null
  }

  static fieldLabel_(code) {
    var row = field_(code)
    if (row == null) return "Unknown field %(code)"
    var reversed = (code < 0) != row[2]
    return reversed ? "%(row[1]) (reversed)" : row[1]
  }

  static profileLabels_(profiles, active) {
    var labels = []
    for (i in 0...profiles.count) {
      var marker = i == active ? "* " : "  "
      labels.add("%(marker)%(profiles[i][0])")
    }
    labels.add("")
    return labels
  }

  static show(app) {
    var clipboard = null
    while (true) {
      var profiles = Menu.sortProfiles
      var active = Menu.activeSortProfile
      var commands = {}
      commands[Key.insert] = ["insert", true]
      commands[Key.delete] = ["delete", false]
      commands[Key.f2] = ["rename", false]
      commands[Key.f5] = ["copy", false]
      commands[Key.ctrlX] = ["cut", false]
      commands[Key.f6] = ["paste", false]
      var picked = MenuUi.commandChoice(app, "Sort Profiles",
          profileLabels_(profiles, active), active, profilesHelp_(), commands)
      if (picked == null) {
        if (!Menu.saveSortProfiles()) {
          Alert.show(app, "Sort Profiles",
              "The sort profiles could not be saved.")
        }
        return
      }
      var command = picked[0]
      var index = picked[1]
      if (command == "insert" ||
          (command == "select" && index == profiles.count)) {
        new_(app, profiles, active, index.min(profiles.count))
      } else if (command == "delete") {
        if (!Menu.deleteSortProfile(index)) failed_(app)
      } else if (command == "rename") {
        rename_(app, index, profiles[index])
      } else if (command == "copy") {
        clipboard = [profiles[index][0], profiles[index][1].toList]
      } else if (command == "cut") {
        clipboard = [profiles[index][0], profiles[index][1].toList]
        if (!Menu.deleteSortProfile(index)) failed_(app)
      } else if (command == "paste") {
        if (clipboard != null) paste_(app, index, clipboard)
      } else if (command == "select") {
        profile_(app, index)
      }
    }
  }

  static profile_(app, index) {
    var profiles = Menu.sortProfiles
    if (index < 0 || index >= profiles.count) return
    var profile = profiles[index]
    var action = MenuUi.choice(app, profile[0], [
      "Use Profile",
      "Edit Fields...",
      "Rename...",
      "Copy...",
      "Delete..."
    ], 0, profileHelp_())
    if (action == null) return
    if (action == 0) {
      if (!Menu.setActiveSortProfile(index)) failed_(app)
    } else if (action == 1) {
      var order = editFields_(app, profile[0], profile[1].toList)
      if (!sameOrder_(order, profile[1]) &&
          !Menu.updateSortProfile(index, profile[0], order)) failed_(app)
    } else if (action == 2) {
      rename_(app, index, profile)
    } else if (action == 3) {
      copy_(app, index, profile)
    } else if (action == 4) {
      if (Confirm.show(app, "Delete sort profile %(profile[0])?") &&
          !Menu.deleteSortProfile(index)) failed_(app)
    }
  }

  static new_(app, profiles, active, index) {
    var name = MenuUi.prompt(app, "New Sort Profile", "Profile name",
        "", 19, false, profileNameHelp_())
    if (name == null) return
    var order = profiles.count > 0 ? profiles[active][1].toList : [1]
    order = editFields_(app, name, order)
    if (!Menu.addSortProfile(index, name, order)) failed_(app)
  }

  static rename_(app, index, profile) {
    var name = MenuUi.prompt(app, "Rename Sort Profile", "Profile name",
        profile[0], 19, false, profileNameHelp_())
    if (name != null &&
        !Menu.updateSortProfile(index, name, profile[1])) failed_(app)
  }

  static copy_(app, index, profile) {
    var name = MenuUi.prompt(app, "Copy Sort Profile", "New profile name",
        profile[0], 19, false, profileNameHelp_())
    if (name == null) return
    if (!Menu.addSortProfile(index + 1, name, profile[1])) failed_(app)
  }

  static paste_(app, index, clipboard) {
    var name = clipboard[0]
    for (profile in Menu.sortProfiles) {
      if (profile[0] == name) {
        name = MenuUi.prompt(app, "Paste Sort Profile", "Profile name",
            name, 19, false, profileNameHelp_())
        if (name == null) return
        break
      }
    }
    if (!Menu.addSortProfile(index, name, clipboard[1])) failed_(app)
  }

  static editFields_(app, name, order) {
    while (true) {
      var labels = order.map {|field| fieldLabel_(field) }.toList
      labels.add("")
      var commands = {}
      commands[Key.insert] = ["insert", true]
      commands[Key.delete] = ["delete", false]
      var picked = MenuUi.commandChoice(app, "Sort Fields: %(name)",
          labels, 0, fieldsHelp_(), commands)
      if (picked == null) return order
      var command = picked[0]
      var index = picked[1]
      if (command == "insert" ||
          (command == "select" && index == order.count)) {
        addField_(app, order, index.min(order.count))
      } else if (command == "delete") {
        if (order.count == 1) {
          Alert.show(app, "Sort Fields",
              "A profile must contain at least one field.")
        } else {
          order.removeAt(index)
        }
      } else if (command == "select") {
        editField_(app, order, index)
      }
    }
  }

  static sameOrder_(left, right) {
    if (left.count != right.count) return false
    for (i in 0...left.count) {
      if (left[i] != right[i]) return false
    }
    return true
  }

  static addField_(app, order, index) {
    var available = []
    for (row in Menu.sortFields) {
      var used = false
      for (field in order) {
        if (field.abs == row[0]) used = true
      }
      if (!used) available.add(row)
    }
    if (available.count == 0) {
      Alert.show(app, "Sort Fields", "All fields are already in this profile.")
      return
    }
    var rows = available.map {|row| [row[0], row[1]] }.toList
    var field = MenuUi.choice(app, "Add Sort Field", rows, null,
        addFieldHelp_())
    if (field != null) order.insert(index, field)
  }

  static editField_(app, order, index) {
    var action = MenuUi.choice(app, fieldLabel_(order[index]), [
      "Reverse Direction",
      "Move Up",
      "Move Down",
      "Remove"
    ], 0, fieldActionHelp_())
    if (action == null) return
    if (action == 0) {
      order[index] = -order[index]
    } else if (action == 1 && index > 0) {
      var previous = order[index - 1]
      order[index - 1] = order[index]
      order[index] = previous
    } else if (action == 2 && index + 1 < order.count) {
      var following = order[index + 1]
      order[index + 1] = order[index]
      order[index] = following
    } else if (action == 3) {
      if (order.count == 1) {
        Alert.show(app, "Sort Fields",
            "A profile must contain at least one field.")
      } else {
        order.removeAt(index)
      }
    }
  }

  static failed_(app) {
    Alert.show(app, "Sort Profiles",
        "The profile could not be changed. Check its name and fields.")
  }

  static profilesHelp_() {
    return "# Sort Profiles\n\n" +
        "Select a profile and press Enter for its actions. Insert or the " +
        "blank final row creates a profile; Delete removes one. F2 renames, " +
        "F5 copies, Ctrl-X cuts, and F6 pastes. Changes update the working " +
        "profiles; the configuration file is written when this screen " +
        "closes. In the main directory, `<` and `>` cycle profiles."
  }

  static profileHelp_() {
    return "# Sort Profile\n\n" +
        "Use Profile\n:  Make this the active directory sort order\n" +
        "Edit Fields\n:  Choose the fields and precedence used for sorting\n" +
        "Rename\n:  Give this profile a unique name\n" +
        "Copy\n:  Create a new profile from these fields\n" +
        "Delete\n:  Remove this profile"
  }

  static profileNameHelp_() {
    return "# Sort Profile Name\n\n" +
        "Enter a unique name for the sort profile."
  }

  static fieldsHelp_() {
    return "# Sort Fields\n\n" +
        "Fields are applied from top to bottom. Enter opens the selected " +
        "field's actions. Insert adds a field before the highlight; the " +
        "blank final row appends one. Delete removes a field."
  }

  static addFieldHelp_() {
    return "# Add Sort Field\n\n" +
        "Select a field that is not already present in this profile."
  }

  static fieldActionHelp_() {
    return "# Sort Field\n\n" +
        "Reverse Direction\n:  Toggle ascending and descending order\n" +
        "Move Up / Move Down\n:  Change this field's precedence\n" +
        "Remove\n:  Delete this field from the profile"
  }

}
