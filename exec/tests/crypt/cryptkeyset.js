var ks = CryptKeyset(system.temp_dir + "tmpkeyset", CryptKeyset.KEYOPT.CREATE);
ks.close();
file_remove(system.temp_dir + "tmpkeyset");
