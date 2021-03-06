testdata := 1024 2048 4096 8192 16384 32768


mkfile_path ?= $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir ?= $(dir $(mkfile_path))
hrf_dir := $(mkfile_dir)/../../../heterorefactor/
instrumentation_profile := /tmp/hetero-profile

# Contention on $(instrumentation_profile) file, disallow parallelization
.NOTPARALLEL:
.PHONY: all clean invariant refactor runtime \
		synthesis implement original_runtime compile_time

all: invariant refactor runtime

invariant: $(addprefix $(mkfile_dir)/invariant/invariant_, $(addsuffix .ivr, $(testdata)))
refactor: $(addprefix $(mkfile_dir)/refactored/kernel_device_, $(addsuffix .cpp, $(testdata)))
runtime: invariant original_runtime refactor
synthesis: $(addprefix $(mkfile_dir)/rpt/synthesis_log_, $(addsuffix .txt, $(testdata))) \
		$(mkfile_dir)/rpt/synthesis_log_original.txt
implement: $(addprefix $(mkfile_dir)/rpt/implement_log_, $(addsuffix .txt, $(testdata)))
manual: $(mkfile_dir)/rpt/implement_log_manual.txt

original_runtime: $(addprefix $(mkfile_dir)/rpt/original_run_time_, $(addsuffix .txt, $(testdata)))
compile_time: $(mkfile_dir)/bin/original.exe $(mkfile_dir)/bin/instrument.exe

$(addprefix $(mkfile_dir)/invariant/invariant_, $(addsuffix .ivr, $(testdata))): \
		$(mkfile_dir)/bin/instrument.exe \
		$(mkfile_dir)/testdata/data_generator.sh
	mkdir -p $(mkfile_dir)/tmp
	mkdir -p $(mkfile_dir)/rpt
	mkdir -p $(mkfile_dir)/invariant
	$(eval @_base := $(basename $(notdir $@)))
	$(eval @_tmp := $(@_base:%.ivr=%))
	$(eval @_size := $(@_tmp:invariant_%=%))
	$(mkfile_dir)/testdata/data_generator.sh $(@_size) |  \
		time -o $(mkfile_dir)/rpt/instrument_run_time_$(@_size).txt \
		$(mkfile_dir)/bin/instrument.exe > \
		$(mkfile_dir)/tmp/instrument_output_$(@_size).txt ; \
	awk -f $(hrf_dir)/instrumentation/recursive/generate_invariant.awk \
		$(instrumentation_profile) \
		> $(mkfile_dir)/invariant/invariant_$(@_size).ivr ; \

$(addprefix $(mkfile_dir)/rpt/original_run_time_, $(addsuffix .txt, $(testdata))): \
		$(mkfile_dir)/bin/original.exe \
		$(mkfile_dir)/testdata/data_generator.sh
	mkdir -p $(mkfile_dir)/tmp
	mkdir -p $(mkfile_dir)/rpt
	mkdir -p $(mkfile_dir)/invariant
	$(eval @_base := $(basename $(notdir $@)))
	$(eval @_tmp := $(@_base:%.txt=%))
	$(eval @_size := $(@_tmp:original_run_time_%=%))
	$(mkfile_dir)/testdata/data_generator.sh $(@_size) |  \
		time -o $(mkfile_dir)/rpt/original_run_time_$(@_size).txt \
		$(mkfile_dir)/bin/original.exe > \
		$(mkfile_dir)/tmp/original_output_$(@_size).txt ; \

$(addprefix $(mkfile_dir)/refactored/kernel_device_, $(addsuffix .cpp, $(testdata))): \
		$(mkfile_dir)/src/kernel.cpp \
		$(addprefix $(mkfile_dir)/invariant/invariant_, $(addsuffix .ivr, $(testdata)))
	mkdir -p $(mkfile_dir)/rpt
	mkdir -p $(mkfile_dir)/refactored
	$(eval @_base := $(basename $(notdir $@)))
	$(eval @_tmp := $(@_base:%.cpp=%))
	$(eval @_size := $(@_tmp:kernel_device_%=%))
	time -o $(mkfile_dir)/rpt/refactoring_time_$(@_size).txt \
		$(hrf_dir)/refactoring/build/heterorefactor \
		-rec -recfile $(mkfile_dir)/invariant/invariant_$(@_size).ivr \
		-u $@ $<

$(addprefix $(mkfile_dir)/rpt/synthesis_log_, $(addsuffix .txt, $(testdata))): \
		$(addprefix $(mkfile_dir)/refactored/kernel_device_, $(addsuffix .cpp, $(testdata)))
	mkdir -p $(mkfile_dir)/rpt
	mkdir -p $(mkfile_dir)/refactored
	$(eval @_base := $(basename $(notdir $@)))
	$(eval @_tmp := $(@_base:%.txt=%))
	$(eval @_size := $(@_tmp:synthesis_log_%=%))
	-rm -rf $(mkfile_dir)/tmp/synthesis_$(@_size)
	mkdir -p $(mkfile_dir)/tmp/synthesis_$(@_size)
	cp $(mkfile_dir)/refactored/kernel_device_$(@_size).cpp \
		$(mkfile_dir)/tmp/synthesis_$(@_size)/kernel.cpp
	cp $(mkfile_dir)/../commons/hls.tcl $(mkfile_dir)/tmp/synthesis_$(@_size)/hls.tcl
	cd $(mkfile_dir)/tmp/synthesis_$(@_size) && vivado_hls -f hls.tcl
	cp $(mkfile_dir)/tmp/synthesis_$(@_size)/vivado_hls.log \
		$(mkfile_dir)/rpt/synthesis_log_$(@_size).txt
	cp $(mkfile_dir)/tmp/synthesis_$(@_size)/device/device/syn/report/process_top_csynth.rpt \
		$(mkfile_dir)/rpt/synthesis_report_$(@_size).txt

$(mkfile_dir)/rpt/synthesis_log_original.txt: $(mkfile_dir)/src/kernel.cpp
	mkdir -p $(mkfile_dir)/rpt
	-rm -rf $(mkfile_dir)/tmp/synthesis_original
	mkdir -p $(mkfile_dir)/tmp/synthesis_original
	cp $(mkfile_dir)/src/kernel.cpp $(mkfile_dir)/tmp/synthesis_original/kernel.cpp
	cp $(mkfile_dir)/../commons/hls.tcl $(mkfile_dir)/tmp/synthesis_original/hls.tcl
	# synthesis will fail
	-cd $(mkfile_dir)/tmp/synthesis_original && vivado_hls -f hls.tcl
	cp $(mkfile_dir)/tmp/synthesis_original/vivado_hls.log \
		$(mkfile_dir)/rpt/synthesis_log_original.txt

$(addprefix $(mkfile_dir)/rpt/implement_log_, $(addsuffix .txt, $(testdata))): \
		$(addprefix $(mkfile_dir)/refactored/kernel_device_, $(addsuffix .cpp, $(testdata)))
	mkdir -p $(mkfile_dir)/rpt
	mkdir -p $(mkfile_dir)/refactored
	$(eval @_base := $(basename $(notdir $@)))
	$(eval @_tmp := $(@_base:%.txt=%))
	$(eval @_size := $(@_tmp:implement_log_%=%))
	-rm -rf $(mkfile_dir)/tmp/implement_$(@_size)
	mkdir -p $(mkfile_dir)/tmp/implement_$(@_size)
	cp $(mkfile_dir)/refactored/kernel_device_$(@_size).cpp \
		$(mkfile_dir)/tmp/implement_$(@_size)/kernel.cpp
	cp $(mkfile_dir)/../commons/impl.tcl $(mkfile_dir)/tmp/implement_$(@_size)/impl.tcl
	# it is possible that implementation fail for large designs
	-cd $(mkfile_dir)/tmp/implement_$(@_size) && \
		time -o $(mkfile_dir)/rpt/implement_time_$(@_size).txt \
			vivado_hls -f impl.tcl
	cp $(mkfile_dir)/tmp/implement_$(@_size)/vivado_hls.log \
		$(mkfile_dir)/rpt/implement_log_$(@_size).txt
	-cp $(mkfile_dir)/tmp/implement_$(@_size)/device/device/impl/report/verilog/process_top_export.rpt \
		$(mkfile_dir)/rpt/implement_report_$(@_size).txt

$(mkfile_dir)/rpt/implement_log_manual.txt: \
		$(mkfile_dir)/src/manual-kernel.cpp
	mkdir -p $(mkfile_dir)/rpt
	mkdir -p $(mkfile_dir)/refactored
	-rm -rf $(mkfile_dir)/tmp/implement_manual
	mkdir -p $(mkfile_dir)/tmp/implement_manual
	cp $(mkfile_dir)/src/manual-kernel.cpp \
		$(mkfile_dir)/tmp/implement_manual/kernel.cpp
	cp $(mkfile_dir)/../commons/impl.tcl $(mkfile_dir)/tmp/implement_manual/impl.tcl
	# it is possible that implementation fail for large designs
	-cd $(mkfile_dir)/tmp/implement_manual && \
		time -o $(mkfile_dir)/rpt/implement_time_manual.txt \
			vivado_hls -f impl.tcl
	cp $(mkfile_dir)/tmp/implement_manual/vivado_hls.log \
		$(mkfile_dir)/rpt/implement_log_manual.txt
	-cp $(mkfile_dir)/tmp/implement_manual/device/device/impl/report/verilog/process_top_export.rpt \
		$(mkfile_dir)/rpt/implement_report_manual.txt

$(mkfile_dir)/bin/original.exe: \
		$(mkfile_dir)/src/kernel.cpp \
		$(mkfile_dir)/src/testbed.cpp
	mkdir -p $(mkfile_dir)/bin
	mkdir -p $(mkfile_dir)/rpt
	time -o $(mkfile_dir)/rpt/original_compilation_time.txt \
		g++ -o $@ $^

$(mkfile_dir)/bin/instrument.exe: \
		$(mkfile_dir)/refactored/kernel_instrument.cpp \
		$(mkfile_dir)/src/testbed.cpp
	mkdir -p $(mkfile_dir)/bin
	mkdir -p $(mkfile_dir)/rpt
	time -o $(mkfile_dir)/rpt/instrument_compilation_time.txt \
		g++ -o $@ $^

$(mkfile_dir)/refactored/kernel_instrument.cpp: $(mkfile_dir)/src/kernel.cpp
	mkdir -p $(mkfile_dir)/refactored
	mkdir -p $(mkfile_dir)/rpt
	time -o $(mkfile_dir)/rpt/instrument_refactoring_time.txt \
		$(hrf_dir)/refactoring/build/heterorefactor \
		-instrument $(instrumentation_profile) \
		-u $@ $^

clean:
	-rm -rf $(mkfile_dir)/refactored
	-rm -rf $(mkfile_dir)/invariant
	-rm -rf $(mkfile_dir)/bin
	-rm -rf $(mkfile_dir)/tmp
	-rm -rf $(mkfile_dir)/rpt
