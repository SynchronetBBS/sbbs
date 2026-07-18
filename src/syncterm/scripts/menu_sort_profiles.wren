import "syncterm_menu" for Menu
import "syncterm" for Key
import "menu_ui" for MenuUi
import "ui_popup" for Alert

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

  static profileLabels_(profiles) {
    var labels = []
    for (profile in profiles) labels.add(profile[0])
    labels.add("")
    return labels
  }

  static nameUsed_(name, except) {
    var profiles = Menu.sortProfiles
    for (i in 0...profiles.count) {
      if (i != except && MenuUi.namesEqual(profiles[i][0], name)) return true
    }
    return false
  }

  static show(app) {
    var clipboard = null
    var selected = Menu.activeSortProfile
    while (true) {
      var profiles = Menu.sortProfiles
      var commands = {}
      commands[Key.insert] = ["insert", true]
      commands[Key.delete] = ["delete", false]
      commands[Key.f2] = ["rename", false]
      commands[Key.f5] = ["copy", false]
      commands[Key.ctrlX] = ["cut", false]
      commands[Key.f6] = ["paste", false]
      var picked = MenuUi.commandChoice(app, "Sort Profiles",
          profileLabels_(profiles), selected, profilesHelp_(), commands)
      if (picked == null) {
        if (!Menu.saveSortProfiles()) {
          Alert.show(app, "Sort Profiles",
              "The sort profiles could not be saved.")
        }
        return
      }
      var command = picked[0]
      var index = picked[1]
      selected = index
      if (command == "insert" ||
          (command == "select" && index == profiles.count)) {
        new_(app, index.min(profiles.count))
      } else if (command == "delete") {
        if (!Menu.deleteSortProfile(index)) {
          failed_(app)
        } else {
          selected = index.min(Menu.sortProfiles.count)
        }
      } else if (command == "rename") {
        rename_(app, index, profiles[index])
      } else if (command == "copy") {
        clipboard = [profiles[index][0], profiles[index][1].toList]
      } else if (command == "cut") {
        clipboard = [profiles[index][0], profiles[index][1].toList]
        if (!Menu.deleteSortProfile(index)) {
          failed_(app)
        } else {
          selected = index.min(Menu.sortProfiles.count)
        }
      } else if (command == "paste") {
        if (clipboard != null) paste_(app, index, clipboard)
      } else if (command == "select") {
        selected = editProfiles_(app, index)
      }
    }
  }

  static editProfiles_(app, index) {
    while (true) {
      var profiles = Menu.sortProfiles
      if (profiles.count == 0) return 0
      index = ((index % profiles.count) + profiles.count) % profiles.count
      var profile = profiles[index]
      var result = editFields_(app, profile[0], profile[1].toList)
      if (!sameOrder_(result[0], profile[1]) &&
          !Menu.updateSortProfile(index, profile[0], result[0])) {
        failed_(app)
        return index
      }
      if (result[1] == 0) return index
      index = index + result[1]
    }
  }

  static new_(app, index) {
    var name = ""
    while (true) {
      name = MenuUi.prompt(app, "New Sort Profile", "Profile name",
          name, 19, false, profileNameHelp_())
      if (name == null || name.count == 0) return
      if (!nameUsed_(name, -1)) break
      Alert.show(app, "New Sort Profile", "Duplicate profile name.")
    }
    var result = editFields_(app, name, [1])
    if (!Menu.addSortProfile(index, name, result[0])) failed_(app)
  }

  static rename_(app, index, profile) {
    var name = MenuUi.prompt(app, "Rename Sort Profile", "Profile name",
        profile[0], 19, false, profileNameHelp_())
    if (name == null || name.count == 0 ||
        MenuUi.namesEqual(name, profile[0])) return
    if (nameUsed_(name, index)) {
      Alert.show(app, "Rename Sort Profile", "Duplicate profile name.")
      return
    }
    if (!Menu.updateSortProfile(index, name, profile[1])) failed_(app)
  }

  static paste_(app, index, clipboard) {
    var name = clipboard[0]
    if (nameUsed_(name, -1)) {
      while (true) {
        name = MenuUi.prompt(app, "Paste Sort Profile", "Profile name",
            name, 19, false, profileNameHelp_())
        if (name == null || name.count == 0) return
        if (!nameUsed_(name, -1)) break
        Alert.show(app, "Paste Sort Profile", "Duplicate profile name.")
      }
    }
    if (!Menu.addSortProfile(index, name, clipboard[1])) failed_(app)
  }

  static editFields_(app, name, order) {
    var selected = 0
    while (true) {
      var labels = order.map {|field| fieldLabel_(field) }.toList
      labels.add("")
      var commands = {}
      commands[Key.insert] = ["insert", true]
      commands[Key.delete] = ["delete", false]
      commands[0x5B] = ["previous", true]
      commands[0x5D] = ["next", true]
      var picked = MenuUi.commandChoice(app, "Sort Fields: %(name)",
          labels, selected, fieldsHelp_(), commands)
      if (picked == null) return [order, 0]
      var command = picked[0]
      var index = picked[1]
      selected = index
      if (command == "previous") return [order, -1]
      if (command == "next") return [order, 1]
      if (command == "insert" ||
          (command == "select" && index == order.count)) {
        addField_(app, order, index.min(order.count))
      } else if (command == "delete") {
        if (index < order.count) order.removeAt(index)
      } else if (command == "select") {
        order[index] = -order[index]
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

  static failed_(app) {
    Alert.show(app, "Sort Profiles",
        "The profile could not be changed. Check its name and fields.")
  }

  static profilesHelp_() {
    return "# Sort Profiles\n\n" +
        "Select a profile and press Enter to edit its fields. Insert or the " +
        "blank final row creates a profile; Delete removes one. F2 renames, " +
        "F5 copies, Ctrl-X cuts, and F6 pastes. Changes update the working " +
        "profiles; the configuration file is written when this screen " +
        "closes. In the main directory, `<` and `>` cycle profiles."
  }

  static profileNameHelp_() {
    return "# Sort Profile Name\n\n" +
        "Enter a unique name for the sort profile."
  }

  static fieldsHelp_() {
    return "# Sort Fields\n\n" +
        "Fields are applied from top to bottom. Enter reverses the selected " +
        "field. Insert adds a field before the highlight; the blank final " +
        "row appends one. Delete removes a field. Use `[` and `]` to edit " +
        "the previous or next profile."
  }

  static addFieldHelp_() {
    return "# Add Sort Field\n\n" +
        "Select a field that is not already present in this profile."
  }

}
