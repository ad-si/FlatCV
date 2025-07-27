FROM gcc:14
COPY . .
RUN make --always-make flatcv

# Keep container alive for `docker cp`
CMD ["true"]
