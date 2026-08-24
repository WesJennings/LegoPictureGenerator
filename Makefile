# Lego Picture Generator — local website + CLI
#
#   make setup   - resolve backend deps + npm install
#   make dev     - run API (:8080) and Vite dev server (:5173)
#   make test    - backend tests + frontend type-check/build
#   make build   - production build (web/dist + shaded backend JAR)
#   make start   - single Java process serving API + built frontend
#   make cli     - run the offline CLI on samples/Jarvis.png

MVNW = cd backend && ./mvnw

.PHONY: setup dev test build start cli clean

setup:
	$(MVNW) -q dependency:resolve
	cd web && npm install

dev:
	@echo "API on http://127.0.0.1:8080 — start 'cd web && npm run dev' in another terminal for the UI (http://localhost:5173)"
	cd backend && LEGO_DB_PATH=../data/bricks.db LEGO_JOBS_PATH=../runtime/jobs \
		./mvnw -q compile exec:java -Dexec.mainClass=com.legopicturegenerator.Application

test:
	$(MVNW) test
	cd web && npm run build

build:
	cd web && npm run build
	$(MVNW) -q package -DskipTests

start: build
	java -Xmx1g -jar backend/target/lego-picture-generator-0.1.0.jar

cli:
	cd backend && LEGO_DB_PATH=../data/bricks.db \
		./mvnw -q compile exec:java -Dexec.mainClass=com.legopicturegenerator.cli.CliApplication \
		-Dexec.args="../samples/Jarvis.png ../runtime/cli 80 greedy"

clean:
	$(MVNW) -q clean
	rm -rf web/dist web/node_modules runtime
