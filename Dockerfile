FROM eclipse-temurin:17-jdk AS builder
COPY . /work
RUN apt-get update -q && apt-get install -y -q gcc make
RUN make -C /work

FROM scratch
COPY --from=builder /work/libjvmkill.so /
