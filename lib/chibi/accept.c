
/* chibi-ffi should probably be able to detect these patterns automatically, */
/* but for now we manually check two special cases - accept should check for */
/* EWOULDBLOCK and block on the socket, and listen should automatically make */
/* sockets non-blocking. */

sexp sexp_accept (sexp ctx, sexp self, int sock, struct sockaddr* addr, socklen_t len) {
#if SEXP_USE_GREEN_THREADS
  sexp f;
#endif
  int res;
  res = accept(sock, addr, &len);
#if SEXP_USE_GREEN_THREADS
  if (res < 0 && errno == EWOULDBLOCK) {
    f = sexp_global(ctx, SEXP_G_THREADS_BLOCKER);
    if (sexp_applicablep(f)) {
      sexp_apply2(ctx, f, sexp_make_fixnum(sock), SEXP_FALSE);
      return sexp_global(ctx, SEXP_G_IO_BLOCK_ERROR);
    }
  }
  if (res >= 0)
    fcntl(res, F_SETFL, fcntl(res, F_GETFL) | O_NONBLOCK);
#endif
  return sexp_make_fileno(ctx, sexp_make_fixnum(res), SEXP_FALSE);
}

/* likewise sendto and recvfrom should suspend the thread gracefully */

#define sexp_zerop(x) ((x) == SEXP_ZERO || (sexp_flonump(x) && sexp_flonum_value(x) == 0.0))

sexp sexp_sendto (sexp ctx, sexp self, int sock, const void* buffer, size_t len, int flags, struct sockaddr* addr, socklen_t addr_len, sexp timeout) {
#if SEXP_USE_GREEN_THREADS
  sexp f;
#endif
  ssize_t res;
  res = sendto(sock, buffer, len, flags, addr, addr_len);
#if SEXP_USE_GREEN_THREADS
  if (res < 0 && errno == EWOULDBLOCK && !sexp_zerop(timeout)) {
    f = sexp_global(ctx, SEXP_G_THREADS_BLOCKER);
    if (sexp_applicablep(f)) {
      sexp_apply2(ctx, f, sexp_make_fixnum(sock), timeout);
      return sexp_global(ctx, SEXP_G_IO_BLOCK_ERROR);
    }
  }
#endif
  return sexp_make_fixnum(res);
}

sexp sexp_recvfrom (sexp ctx, sexp self, int sock, void* buffer, size_t len, int flags, struct sockaddr* addr, socklen_t addr_len, sexp timeout) {
#if SEXP_USE_GREEN_THREADS
  sexp f;
#endif
  ssize_t res;
  res = recvfrom(sock, buffer, len, flags, addr, &addr_len);
#if SEXP_USE_GREEN_THREADS
  if (res < 0 && errno == EWOULDBLOCK && !sexp_zerop(timeout)) {
    f = sexp_global(ctx, SEXP_G_THREADS_BLOCKER);
    if (sexp_applicablep(f)) {
      sexp_apply2(ctx, f, sexp_make_fixnum(sock), timeout);
      return sexp_global(ctx, SEXP_G_IO_BLOCK_ERROR);
    }
  }
#endif
  return sexp_make_fixnum(res);
}

/* If we're listening on a socket from Scheme, we most likely want it */
/* to be non-blocking. */

sexp sexp_listen (sexp ctx, sexp self, sexp fileno, sexp backlog) {
  int fd, res;
  sexp_assert_type(ctx, sexp_filenop, SEXP_FILENO, fileno);
  sexp_assert_type(ctx, sexp_fixnump, SEXP_FIXNUM, backlog);
  fd = sexp_fileno_fd(fileno);
  res = listen(fd, sexp_unbox_fixnum(backlog));
#if SEXP_USE_GREEN_THREADS
  if (res >= 0)
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#endif
  return (res == 0) ? SEXP_TRUE : SEXP_FALSE;
}

/* Additional utilities. */

sexp sexp_sockaddr_name (sexp ctx, sexp self, struct sockaddr* addr) {
  char buf[24];
  /* struct sockaddr_in *sa = (struct sockaddr_in *)addr; */
  /* unsigned char *ptr = (unsigned char *)&(sa->sin_addr); */
  /* sprintf(buf, "%d.%d.%d.%d", ptr[0], ptr[1], ptr[2], ptr[3]); */
  inet_ntop(addr->sa_family,
            (addr->sa_family == AF_INET6 ?
             (void*)(&(((struct sockaddr_in6 *)addr)->sin6_addr)) :
             (void*)(&(((struct sockaddr_in *)addr)->sin_addr))),
            buf, 24);
  return sexp_c_string(ctx, buf, -1);
}

int sexp_sockaddr_port (sexp ctx, sexp self, struct sockaddr* addr) {
  struct sockaddr_in *sa = (struct sockaddr_in *)addr;
  return sa->sin_port;
}
