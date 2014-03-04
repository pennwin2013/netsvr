.PHONY: clean All

All:
	@echo "----------Building project:[ netsvr - Debug ]----------"
	@$(MAKE) -f  "netsvr.mk"
clean:
	@echo "----------Cleaning project:[ netsvr - Debug ]----------"
	@$(MAKE) -f  "netsvr.mk" clean
