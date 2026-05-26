.PHONY: all device simulator package clean

all: package

device:
	$(MAKE) -C testlib device
	$(MAKE) -C testhost device

simulator:
	$(MAKE) -C testlib simulator
	$(MAKE) -C testhost simulator

package:
	$(MAKE) -C testlib
	$(MAKE) -C testhost
	@echo "[pkg] staging ball library into TestHost.pdx"
	@cp -f testlib/TestLib.pdx/pdex.bin   testhost/TestHost.pdx/ball.bin   2>/dev/null || true
	@cp -f testlib/TestLib.pdx/pdex.so    testhost/TestHost.pdx/ball.so    2>/dev/null || true
	@cp -f testlib/TestLib.pdx/pdex.dylib testhost/TestHost.pdx/ball.dylib 2>/dev/null || true
	@cp -f testlib/TestLib.pdx/pdex.dll   testhost/TestHost.pdx/ball.dll   2>/dev/null || true
	@ls -1 testhost/TestHost.pdx/ball.* 2>/dev/null || echo "[pkg] (no ball.* artifacts found)"

clean:
	$(MAKE) -C testlib clean
	$(MAKE) -C testhost clean
