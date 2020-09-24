all : hello_query hello_plan hello_exec hello_bgw

.PHONY : hello_query hello_plan hello_exec hello_bgw install

hello_query:
	@cd 01_hello_query; make

hello_plan:
	@cd 02_hello_plan; make

hello_exec:
	@cd 03_hello_exec; make

hello_bgw:
	@cd 04_hello_bgw; make

install:
	@cd 01_hello_query; make install
	@cd 02_hello_plan; make install
	@cd 03_hello_exec; make install
	@cd 04_hello_bgw; make install

clean:
	@cd 01_hello_query; make clean
	@cd 02_hello_plan; make clean
	@cd 03_hello_exec; make clean
	@cd 04_hello_bgw; make clean
