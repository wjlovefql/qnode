/*
 * See Copyright Notice in qnode.h
 */

#include "qactor.h"
#include "qassert.h"
#include "qdefines.h"
#include "qdict.h"
#include "qengine.h"
#include "qlog.h"
#include "qluautil.h"
#include "qnet.h"
#include "qserver.h"
#include "qsocket.h"
#include "qstring.h"
#include "qthread.h"

static void init_tcp_listen_params(qactor_t *actor) {
  actor->listen_params = qdict_new(5);

  {
    qkey_t key;
    QKEY_STRING(key, "packet");
    qdict_val_t val;
    QVAL_NUMBER(val, 0);
    qdict_add(actor->listen_params, &key, &val);
  }
}

static int qnode_tcp_listen(lua_State *state) {
  qactor_t *actor = qlua_get_actor(state);
  qassert(actor);
  qassert(actor->listen_fd == 0);
  //const char *addr = lua_tostring(state, 1);
  const char *addr = "0.0.0.0";
  int port = (int)lua_tonumber(state, 1);
  if (lua_type(state, 2) != LUA_TFUNCTION) {
    qerror("invalid listener on :%d\n", addr, port);
    return 0;
  }
  if (actor->lua_ref[LISTENER] != -1) {
    qerror("listener exist on actor listen to %s:%d\n", addr, port);
    return 0;
  }
  /* push listen callback */
  lua_pushvalue(state, 2);
  actor->lua_ref[LISTENER] = luaL_ref(state, LUA_REGISTRYINDEX);
  if (actor->lua_ref[LISTENER] == LUA_REFNIL) {
    qerror("ref listener on %s:%d error\n", addr, port);
    return 0;
  }
  /* pop listen callback */
  lua_pop(state, 1);
  int fd = qnet_tcp_listen(port, addr);
  if (fd < 0) {
    qerror("listen on %s:%d error\n", addr, port);
    return 0;
  }
  qthread_t *thread = g_server->threads[actor->tid];
  qengine_t *engine = thread->engine;
  if (qengine_add_event(engine, fd, QEVENT_READ, qactor_accept, actor) < 0) {
    qerror("add event on %s:%d error\n", addr, port);
    return 0;
  }
  actor->listen_fd = fd;
  lua_pushvalue(state, -3);

  init_tcp_listen_params(actor);  
  qdict_copy_lua_table(actor->listen_params, state, 3);
  return 0;
}

static int qnode_tcp_accept(lua_State *state) {
  qactor_t *actor = qlua_get_actor(state);
  qsocket_t *socket = qactor_get_socket(actor);
  lua_pushlightuserdata(actor->state, socket);
  return 1;
}

static int qnode_tcp_recv(lua_State *state) {
  qsocket_t *socket = (qsocket_t*)lua_touserdata(state, 1);
  UNUSED(socket);
  return 0;
}

static int qnode_tcp_send(lua_State *state) {
  UNUSED(state);
  return 0;
}

luaL_Reg net_apis[] = {
  {"qnode_tcp_listen",  qnode_tcp_listen},
  {"qnode_tcp_accept",  qnode_tcp_accept},
  {"qnode_tcp_recv",    qnode_tcp_recv},
  {"qnode_tcp_send",    qnode_tcp_send},
  {NULL, NULL},
};