# objects.mk

# Make 'include file' listing object files for Synchronet SCFG

# MTOBJODIR and OFILE must be pre-defined

OBJS =	$(MTOBJODIR)/scfg$(OFILE)\
	$(MTOBJODIR)/scfgxtrn$(OFILE)\
	$(MTOBJODIR)/scfgmsg$(OFILE)\
	$(MTOBJODIR)/scfgnet$(OFILE)\
	$(MTOBJODIR)/scfgnode$(OFILE)\
	$(MTOBJODIR)/scfgsub$(OFILE)\
	$(MTOBJODIR)/scfgsys$(OFILE)\
	$(MTOBJODIR)/scfgxfr1$(OFILE)\
	$(MTOBJODIR)/scfgxfr2$(OFILE)\
	$(MTOBJODIR)/scfgchat$(OFILE)\
	$(MTOBJODIR)/scfgsave$(OFILE)\
	$(MTOBJODIR)/scfglib1$(OFILE)\
	$(MTOBJODIR)/scfglib2$(OFILE)\
	$(MTOBJODIR)/load_cfg$(OFILE)\
	$(MTOBJODIR)/readtext$(OFILE)\
	$(MTOBJODIR)/text_defaults$(OFILE)\
	$(MTOBJODIR)/ars$(OFILE)\
	$(MTOBJODIR)/nopen$(OFILE)\
	$(MTOBJODIR)/dat_rec$(OFILE)\
	$(MTOBJODIR)/userdat$(OFILE)\
	$(MTOBJODIR)/getstats$(OFILE)\
	$(MTOBJODIR)/msgdate$(OFILE)\
	$(MTOBJODIR)/date_str$(OFILE)\
	$(MTOBJODIR)/str_util$(OFILE)
