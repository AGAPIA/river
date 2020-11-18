version=0.1

build:
	docker image build -t river:${version} --build-arg VERSION=${version} .

run: build
	docker run --rm --name river3 river:${version}

bash:
	docker exec -it river3 /bin/bash

clean:
	docker rm -f river3
	docker image rm river:${version}
