struct mosquitto;

int mqtt_init(struct mosquitto **handle, const char *host, int port,
	      const char *username, const char *password);

void mqtt_close(struct mosquitto *mosquitto);
int mqtt_loop(struct mosquitto *mosquitto);
