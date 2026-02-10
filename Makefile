.PHONY: lint
lint:
	@echo "Running Arduino Lint..."
	arduino-lint $$(basename $$(PWD)) --verbose

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make lint     - Run Arduino Lint on the library"
	@echo "  make help     - Display this help message"
