// Static functions "shared" between client and server

static uint32_t
get32(SFTP_STATIC_TYPE state)
{
	return sftp_get32(state->rxp);
}

static sftp_str_t
getstring(SFTP_STATIC_TYPE state)
{
	return sftp_getstring(state->rxp);
}

static bool
append32(SFTP_STATIC_TYPE state, uint32_t u)
{
	return sftp_append32(&state->txp, u);
}

static bool
check_state(SFTP_STATIC_TYPE state)
{
	assert(state);
	if (!state)
		return false;
	return true;
}

static bool
appendheader(SFTP_STATIC_TYPE state, uint8_t type)
{
	if (!sftp_tx_pkt_reset(&state->txp))
		return false;
	if (!sftp_appendbyte(&state->txp, type))
		return false;
	if (type != SSH_FXP_INIT) {
		if (!sftp_append32(&state->txp, ++state->id))
			return false;
	}
	return true;
}

static bool
enter_function(SFTP_STATIC_TYPE state)
{
	if (!check_state(state))
		return false;
	if (pthread_mutex_lock(&state->mtx))
		return false;
	if (state->terminating) {
		pthread_mutex_unlock(&state->mtx);
		return false;
	}
	state->running++;
	return true;
}

static bool
exit_function(SFTP_STATIC_TYPE state, bool retval)
{
	assert(state->running > 0);
	state->running--;
	pthread_mutex_unlock(&state->mtx);
	return retval;
}
