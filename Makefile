# Lego Picture Generator — local website + CLI
#
#   make setup   - build C++ host + engine, npm install
#   make dev     - run API (:8080); start Vite separately for hot UI
#   make test    - native + host tests + frontend type-check/build
#   make build   - production web/dist + C++ binaries
#   make start   - one C++ process serving API + built frontend
#   make cli     - offline CLI on samples/Jarvis.png

.PHONY: setup dev test build start cli clean

setup:
	bash native/build.sh
	cd web && npm install

dev:
	@echo "API on http://127.0.0.1:8080 — start 'cd web && npm run dev' in another terminal for the UI (http://localhost:5173)"
	LEGO_DB_PATH=data/bricks.db LEGO_JOBS_PATH=runtime/jobs \
		./native/build/lego_server

test:
	bash native/build.sh
	cd web && npm run build

build:
	cd web && npm run build
	bash native/build.sh

start: build
	LEGO_DB_PATH=data/bricks.db LEGO_JOBS_PATH=runtime/jobs \
		./native/build/lego_server

cli:
	LEGO_DB_PATH=data/bricks.db ./native/build/lego_cli samples/Jarvis.png runtime/cli 80 greedy

clean:
	rm -rf web/dist web/node_modules runtime native/build
