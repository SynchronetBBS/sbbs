var ks = CryptKeyset("tmpkeyset", CryptKeyset.KEYOPT.CREATE);
ks.close();
file_remove("tmpkeyset");
