$(call add-hdrs,fd_hfork.h)
$(call add-objs,fd_hfork,fd_choreo)
ifdef FD_HAS_HOSTED
$(call make-unit-test,test_hfork,test_hfork,fd_choreo fd_flamenco fd_tango fd_ballet fd_util)
$(call make-unit-test,test_hfork_sticky_flag,test_hfork_sticky_flag,fd_choreo fd_flamenco fd_tango fd_ballet fd_util)
$(call run-unit-test,test_hfork)
$(call run-unit-test,test_hfork_sticky_flag)
endif
