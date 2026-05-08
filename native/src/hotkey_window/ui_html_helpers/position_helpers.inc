static const char* kbo_webview_position_label_from_values(uint8_t position, uint8_t role)
{
    if (position == 1u) {
        if (role == 11u) {
            return "SP";
        }
        if (role == 12u) {
            return "RP";
        }
        if (role == 13u) {
            return "CL";
        }
        return "P";
    }
    switch (position) {
    case 2u:  return "C";
    case 3u:  return "1B";
    case 4u:  return "2B";
    case 5u:  return "3B";
    case 6u:  return "SS";
    case 7u:  return "LF";
    case 8u:  return "CF";
    case 9u:  return "RF";
    case 10u: return "DH";
    default:  break;
    }

    switch (role) {
    case 1u:  return "P";
    case 2u:  return "C";
    case 3u:  return "1B";
    case 4u:  return "2B";
    case 5u:  return "3B";
    case 6u:  return "SS";
    case 7u:  return "LF";
    case 8u:  return "CF";
    case 9u:  return "RF";
    case 10u: return "DH";
    case 11u: return "SP";
    case 12u: return "RP";
    case 13u: return "CL";
    default:  return "-";
    }
}

static const char* kbo_webview_player_position_label(uint8_t* player, uint8_t fallback_position)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET, sizeof(uint8_t))) {
        return kbo_webview_position_label_from_values(fallback_position, 0u);
    }
    return kbo_webview_position_label_from_values(
        player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
        player[OOTP27_PLAYER_POSITION_ROLE_OFFSET]);
}
