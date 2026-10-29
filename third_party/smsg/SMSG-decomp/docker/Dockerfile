FROM docker.io/library/debian:stable-slim

RUN dpkg --add-architecture i386
RUN apt-get update -y
RUN apt-get install wine wine32 wine32-preloader:i386 xvfb xauth -y

ENV WINEDEBUG=-all

RUN wine wineboot

RUN apt-get install curl unzip ca-certificates -y
RUN curl -sSL -o /tmp/clang-format.whl https://files.pythonhosted.org/packages/32/af/51bb1f77e54863e769d5527a606298da2988520e7d43328648b6d9c6b7b5/clang_format-23.1.3-py2.py3-none-win32.whl \
 && echo "1560b5d7b12c9fe6ace14c659b8fae16d580dfe4abfe18eb02583ba290f01ed0  /tmp/clang-format.whl" | sha256sum -c \
 && mkdir -p /opt/clang-format \
 && unzip -j /tmp/clang-format.whl clang_format/data/bin/clang-format.exe -d /opt/clang-format \
 && rm /tmp/clang-format.whl

COPY entrypoint.sh entrypoint.sh
RUN chmod +x entrypoint.sh
ENTRYPOINT [ "./entrypoint.sh" ]
