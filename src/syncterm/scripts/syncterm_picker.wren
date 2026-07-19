// Picker-VM-only request and path metadata.  These objects expose names and
// stat information but never open files or directories.

class PickerEntry {
  construct from_(raw) {
    _name = raw[0]
    _directory = raw[1]
    _size = raw[2]
    _modified = raw[3]
    _timestamp = raw[4]
  }

  name { _name }
  directory { _directory }
  size { _size }
  modified { _modified }
  timestamp { _timestamp }
}

class PickerListing {
  construct from_(raw) {
    _path = raw[0]
    _root = raw[1]
    _directories = raw[2].map {|entry| PickerEntry.from_(entry) }.toList
    _files = raw[3].map {|entry| PickerEntry.from_(entry) }.toList
  }

  path { _path }
  root { _root }
  directories { _directories }
  files { _files }
}

class PickerPath {
  construct from_(raw) {
    _full = raw[0]
    _directory = raw[1]
    _name = raw[2]
    _exists = raw[3]
    _isDirectory = raw[4]
    _wildcards = raw[5]
  }

  full { _full }
  directory { _directory }
  name { _name }
  exists { _exists }
  isDirectory { _isDirectory }
  wildcards { _wildcards }
}

foreign class PickerRequest {
  foreign mode
  foreign title
  foreign initialPath
  foreign mask
  foreign options

  foreign frameColor
  foreign textColor
  foreign backgroundColor
  foreign inverseColor
  foreign lightbarColor
  foreign lightbarBackgroundColor

  foreign initial_()
  foreign list_(path, mask)
  foreign join_(path, name)
  foreign resolve_(path, text)

  initial {
    var raw = initial_()
    if (raw == null) return null
    return {"path": raw[0], "selected": raw[1]}
  }

  list(path, mask) {
    var raw = list_(path, mask)
    return raw == null ? null : PickerListing.from_(raw)
  }

  join(path, name) { join_(path, name) }

  resolve(path, text) {
    var raw = resolve_(path, text)
    return raw == null ? null : PickerPath.from_(raw)
  }

  foreign accept(path)
  foreign acceptAll(paths)
  foreign acceptCreate(path)
  foreign acceptOverwrite(path)
  foreign cancel()
  foreign quitApplication()
}
