import "react-native-url-polyfill/auto";
import { Buffer } from "buffer";
import process from "process";

global.Buffer = global.Buffer || Buffer;
global.process = global.process || process;
global.navigator = global.navigator || {};
global.navigator.product = "ReactNative";

import AsyncStorage from "@react-native-async-storage/async-storage";
import Ionicons from "@expo/vector-icons/Ionicons";
import React, { useEffect, useMemo, useRef, useState } from "react";
import {
  Alert,
  SafeAreaView,
  ScrollView,
  StatusBar,
  StyleSheet,
  Switch,
  Text,
  TextInput,
  TouchableOpacity,
  View
} from "react-native";

const mqtt = require("mqtt");

const BASE_TOPIC = "porton/device01";
const SETTINGS_KEY = "porton_seguro_iot_settings";

const initialSettings = {
  brokerHost: "192.168.1.100",
  wsPort: "9001",
  username: "",
  password: "",
  autoCloseS: "30",
  maxTravelS: "25",
  ftcWaitS: "4",
  reversePauseS: "2",
  autoCloseSw: true,
  maintenanceSw: false
};

const initialGate = {
  state: "SIN DATOS",
  error: "NONE",
  position_pct: 0,
  online: false,
  lastEvent: "Esperando conexion MQTT",
  sensors: {},
  dips: {}
};

export default function App() {
  const [tab, setTab] = useState("home");
  const [settings, setSettings] = useState(initialSettings);
  const [gate, setGate] = useState(initialGate);
  const [connected, setConnected] = useState(false);
  const [events, setEvents] = useState([]);
  const clientRef = useRef(null);

  useEffect(() => {
    AsyncStorage.getItem(SETTINGS_KEY)
      .then((stored) => {
        if (stored) {
          setSettings({ ...initialSettings, ...JSON.parse(stored) });
        }
      })
      .catch(() => appendEvent("APP", "No se pudo leer la configuracion local"));
  }, []);

  useEffect(() => {
    return () => {
      if (clientRef.current) {
        clientRef.current.end(true);
      }
    };
  }, []);

  const connectionLabel = connected ? "MQTT conectado" : "MQTT desconectado";
  const progress = Math.max(0, Math.min(100, Number(gate.position_pct || 0)));
  const stateTone = gate.state === "ERROR" ? styles.stateError : styles.stateNormal;

  const visibleEvents = useMemo(() => events.slice(0, 40), [events]);

  function appendEvent(type, message) {
    const time = new Date().toLocaleTimeString();
    setEvents((current) => [{ time, type, message }, ...current].slice(0, 80));
  }

  function updateSetting(key, value) {
    setSettings((current) => ({ ...current, [key]: value }));
  }

  async function saveLocalSettings() {
    await AsyncStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
    appendEvent("APP", "Configuracion guardada en el telefono");
  }

  function connectMqtt() {
    if (clientRef.current) {
      clientRef.current.end(true);
      clientRef.current = null;
    }

    const url = `ws://${settings.brokerHost}:${settings.wsPort}/mqtt`;
    const options = {
      clientId: `porton_app_${Math.random().toString(16).slice(2)}`,
      clean: true,
      connectTimeout: 8000,
      reconnectPeriod: 3000
    };

    if (settings.username.trim()) {
      options.username = settings.username.trim();
      options.password = settings.password;
    }

    appendEvent("APP", `Conectando a ${url}`);
    const client = mqtt.connect(url, options);
    clientRef.current = client;

    client.on("connect", () => {
      setConnected(true);
      appendEvent("MQTT", "Conexion establecida");
      client.subscribe(`${BASE_TOPIC}/availability`);
      client.subscribe(`${BASE_TOPIC}/state`);
      client.subscribe(`${BASE_TOPIC}/telemetry`);
      client.subscribe(`${BASE_TOPIC}/event`);
      client.subscribe(`${BASE_TOPIC}/config/state`);
      client.subscribe(`${BASE_TOPIC}/ack`);
    });

    client.on("reconnect", () => appendEvent("MQTT", "Reintentando conexion"));
    client.on("close", () => setConnected(false));
    client.on("error", (error) => appendEvent("MQTT", error.message));
    client.on("message", handleMessage);
  }

  function disconnectMqtt() {
    if (clientRef.current) {
      clientRef.current.end(true);
      clientRef.current = null;
    }
    setConnected(false);
    appendEvent("APP", "Conexion cerrada");
  }

  function handleMessage(topic, payloadBuffer) {
    const payload = payloadBuffer.toString();
    const leaf = topic.replace(`${BASE_TOPIC}/`, "");

    if (leaf === "availability") {
      setGate((current) => ({ ...current, online: payload === "online" }));
      appendEvent("ESP32", `Disponibilidad: ${payload}`);
      return;
    }

    if (leaf === "event" || leaf === "ack") {
      appendEvent(leaf.toUpperCase(), payload);
      setGate((current) => ({ ...current, lastEvent: payload }));
      return;
    }

    try {
      const data = JSON.parse(payload);
      if (leaf === "state") {
        setGate((current) => ({ ...current, ...data, lastEvent: `Estado ${data.state || current.state}` }));
      }
      if (leaf === "telemetry") {
        setGate((current) => ({ ...current, sensors: data.sensors || {}, dips: data.dips || {} }));
      }
      if (leaf === "config/state") {
        setSettings((current) => ({
          ...current,
          autoCloseS: String(data.auto_close_s ?? current.autoCloseS),
          maxTravelS: String(data.max_travel_s ?? current.maxTravelS),
          ftcWaitS: String(data.ftc_wait_s ?? current.ftcWaitS),
          reversePauseS: String(data.reverse_pause_s ?? current.reversePauseS),
          autoCloseSw: Boolean(data.auto_close_sw ?? current.autoCloseSw),
          maintenanceSw: Boolean(data.maintenance_sw ?? current.maintenanceSw)
        }));
        appendEvent("CONFIG", "Configuracion recibida del ESP32");
      }
    } catch {
      appendEvent(leaf.toUpperCase(), payload);
    }
  }

  function publish(topic, message) {
    if (!clientRef.current || !connected) {
      Alert.alert("MQTT desconectado", "Conecta la app al broker antes de enviar.");
      return;
    }
    clientRef.current.publish(topic, message);
  }

  function sendCommand(command) {
    publish(`${BASE_TOPIC}/command`, command);
    appendEvent("APP", `Comando enviado: ${command}`);
  }

  async function sendConfig() {
    await saveLocalSettings();
    const payload = JSON.stringify({
      auto_close_s: Number(settings.autoCloseS),
      max_travel_s: Number(settings.maxTravelS),
      ftc_wait_s: Number(settings.ftcWaitS),
      reverse_pause_s: Number(settings.reversePauseS),
      auto_close_sw: settings.autoCloseSw,
      maintenance_sw: settings.maintenanceSw
    });
    publish(`${BASE_TOPIC}/config/set`, payload);
    appendEvent("APP", "Configuracion enviada al ESP32");
  }

  return (
    <SafeAreaView style={styles.safe}>
      <StatusBar barStyle="dark-content" />
      <View style={styles.header}>
        <View>
          <Text style={styles.title}>Porton Seguro IoT</Text>
          <Text style={styles.subtitle}>{connectionLabel}</Text>
        </View>
        <TouchableOpacity style={[styles.iconButton, connected && styles.iconButtonOn]} onPress={connected ? disconnectMqtt : connectMqtt}>
          <Ionicons name={connected ? "radio" : "radio-outline"} size={24} color={connected ? "#ffffff" : "#16324f"} />
        </TouchableOpacity>
      </View>

      <View style={styles.tabs}>
        <TabButton active={tab === "home"} icon="home-outline" label="Inicio" onPress={() => setTab("home")} />
        <TabButton active={tab === "settings"} icon="options-outline" label="Config" onPress={() => setTab("settings")} />
        <TabButton active={tab === "events"} icon="list-outline" label="Eventos" onPress={() => setTab("events")} />
      </View>

      {tab === "home" && (
        <ScrollView contentContainerStyle={styles.content}>
          <View style={styles.panel}>
            <Text style={styles.label}>Estado actual</Text>
            <Text style={[styles.state, stateTone]}>{gate.state}</Text>
            <Text style={styles.meta}>Error: {gate.error || "NONE"}</Text>
            <View style={styles.progressTrack}>
              <View style={[styles.progressFill, { width: `${progress}%` }]} />
            </View>
            <View style={styles.progressRow}>
              <Text style={styles.meta}>Cerrado</Text>
              <Text style={styles.meta}>{progress}%</Text>
              <Text style={styles.meta}>Abierto</Text>
            </View>
          </View>

          <View style={styles.commandGrid}>
            <CommandButton icon="arrow-up-circle-outline" label="Abrir" tone="primary" onPress={() => sendCommand("OPEN")} />
            <CommandButton icon="arrow-down-circle-outline" label="Cerrar" tone="primary" onPress={() => sendCommand("CLOSE")} />
            <CommandButton icon="stop-circle-outline" label="Stop" tone="danger" onPress={() => sendCommand("STOP")} />
          </View>

          <View style={styles.panel}>
            <Text style={styles.label}>Ultimo evento</Text>
            <Text style={styles.eventText}>{gate.lastEvent}</Text>
          </View>

          <View style={styles.sensorGrid}>
            <StatusPill label="ESP32" active={gate.online} />
            <StatusPill label="F. abierto" active={Boolean(gate.sensors.limit_open)} />
            <StatusPill label="F. cerrado" active={Boolean(gate.sensors.limit_closed)} />
            <StatusPill label="FTC" active={Boolean(gate.sensors.ftc_blocked)} danger />
            <StatusPill label="DIP auto" active={Boolean(gate.dips.auto_close)} />
            <StatusPill label="DIP mant." active={Boolean(gate.dips.maintenance)} danger />
          </View>
        </ScrollView>
      )}

      {tab === "settings" && (
        <ScrollView contentContainerStyle={styles.content}>
          <View style={styles.panel}>
            <Text style={styles.sectionTitle}>Broker MQTT</Text>
            <Field label="IP del broker" value={settings.brokerHost} onChangeText={(value) => updateSetting("brokerHost", value)} />
            <Field label="Puerto WebSocket" value={settings.wsPort} keyboardType="numeric" onChangeText={(value) => updateSetting("wsPort", value)} />
            <Field label="Usuario opcional" value={settings.username} onChangeText={(value) => updateSetting("username", value)} />
            <Field label="Contrasena opcional" value={settings.password} secureTextEntry onChangeText={(value) => updateSetting("password", value)} />
          </View>

          <View style={styles.panel}>
            <Text style={styles.sectionTitle}>Tiempos en segundos</Text>
            <Field label="Auto-cierre" value={settings.autoCloseS} keyboardType="numeric" onChangeText={(value) => updateSetting("autoCloseS", value)} />
            <Field label="Recorrido maximo" value={settings.maxTravelS} keyboardType="numeric" onChangeText={(value) => updateSetting("maxTravelS", value)} />
            <Field label="Espera FTC" value={settings.ftcWaitS} keyboardType="numeric" onChangeText={(value) => updateSetting("ftcWaitS", value)} />
            <Field label="Pausa inversion" value={settings.reversePauseS} keyboardType="numeric" onChangeText={(value) => updateSetting("reversePauseS", value)} />
            <SwitchRow label="Auto-cierre software" value={settings.autoCloseSw} onValueChange={(value) => updateSetting("autoCloseSw", value)} />
            <SwitchRow label="Modo mantenimiento" value={settings.maintenanceSw} onValueChange={(value) => updateSetting("maintenanceSw", value)} />
          </View>

          <View style={styles.commandGrid}>
            <CommandButton icon="save-outline" label="Guardar" tone="neutral" onPress={saveLocalSettings} />
            <CommandButton icon="cloud-upload-outline" label="Enviar" tone="primary" onPress={sendConfig} />
            <CommandButton icon="refresh-outline" label="Reset error" tone="danger" onPress={() => sendCommand("RESET_ERROR")} />
            <CommandButton icon="construct-outline" label="Calibrar" tone="neutral" onPress={() => sendCommand("CALIBRATE")} />
          </View>
        </ScrollView>
      )}

      {tab === "events" && (
        <ScrollView contentContainerStyle={styles.content}>
          {visibleEvents.map((item, index) => (
            <View key={`${item.time}-${index}`} style={styles.eventRow}>
              <Text style={styles.eventTime}>{item.time}</Text>
              <View style={styles.eventBody}>
                <Text style={styles.eventType}>{item.type}</Text>
                <Text style={styles.eventMessage}>{item.message}</Text>
              </View>
            </View>
          ))}
        </ScrollView>
      )}
    </SafeAreaView>
  );
}

function TabButton({ active, icon, label, onPress }) {
  return (
    <TouchableOpacity style={[styles.tabButton, active && styles.tabButtonActive]} onPress={onPress}>
      <Ionicons name={icon} size={20} color={active ? "#ffffff" : "#46627f"} />
      <Text style={[styles.tabLabel, active && styles.tabLabelActive]}>{label}</Text>
    </TouchableOpacity>
  );
}

function CommandButton({ icon, label, tone, onPress }) {
  const style = tone === "danger" ? styles.buttonDanger : tone === "neutral" ? styles.buttonNeutral : styles.buttonPrimary;
  const textStyle = tone === "neutral" ? styles.buttonTextNeutral : styles.buttonText;
  const iconColor = tone === "neutral" ? "#16324f" : "#ffffff";
  return (
    <TouchableOpacity style={[styles.commandButton, style]} onPress={onPress}>
      <Ionicons name={icon} size={28} color={iconColor} />
      <Text style={textStyle}>{label}</Text>
    </TouchableOpacity>
  );
}

function Field({ label, value, onChangeText, keyboardType = "default", secureTextEntry = false }) {
  return (
    <View style={styles.field}>
      <Text style={styles.fieldLabel}>{label}</Text>
      <TextInput
        style={styles.input}
        value={value}
        onChangeText={onChangeText}
        keyboardType={keyboardType}
        secureTextEntry={secureTextEntry}
        autoCapitalize="none"
      />
    </View>
  );
}

function SwitchRow({ label, value, onValueChange }) {
  return (
    <View style={styles.switchRow}>
      <Text style={styles.fieldLabel}>{label}</Text>
      <Switch value={value} onValueChange={onValueChange} trackColor={{ false: "#cfd8e3", true: "#7cc5ff" }} thumbColor={value ? "#0b6fb3" : "#f8fafc"} />
    </View>
  );
}

function StatusPill({ label, active, danger = false }) {
  const activeStyle = danger ? styles.pillDanger : styles.pillActive;
  return (
    <View style={[styles.pill, active && activeStyle]}>
      <Text style={[styles.pillText, active && styles.pillTextActive]}>{label}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  safe: {
    flex: 1,
    backgroundColor: "#f4f7fb"
  },
  header: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    paddingHorizontal: 20,
    paddingTop: 16,
    paddingBottom: 12
  },
  title: {
    color: "#16324f",
    fontSize: 24,
    fontWeight: "800"
  },
  subtitle: {
    color: "#5f748c",
    fontSize: 14,
    marginTop: 2
  },
  iconButton: {
    width: 46,
    height: 46,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: "#c6d4e3",
    alignItems: "center",
    justifyContent: "center",
    backgroundColor: "#ffffff"
  },
  iconButtonOn: {
    backgroundColor: "#0b6fb3",
    borderColor: "#0b6fb3"
  },
  tabs: {
    flexDirection: "row",
    gap: 8,
    paddingHorizontal: 20,
    paddingBottom: 10
  },
  tabButton: {
    flex: 1,
    minHeight: 42,
    borderRadius: 8,
    backgroundColor: "#e8eef6",
    alignItems: "center",
    justifyContent: "center",
    flexDirection: "row",
    gap: 6
  },
  tabButtonActive: {
    backgroundColor: "#16324f"
  },
  tabLabel: {
    color: "#46627f",
    fontWeight: "700"
  },
  tabLabelActive: {
    color: "#ffffff"
  },
  content: {
    padding: 20,
    gap: 14
  },
  panel: {
    backgroundColor: "#ffffff",
    borderRadius: 8,
    padding: 16,
    borderWidth: 1,
    borderColor: "#dce5ef"
  },
  label: {
    color: "#5f748c",
    fontSize: 13,
    fontWeight: "700",
    textTransform: "uppercase"
  },
  sectionTitle: {
    color: "#16324f",
    fontSize: 18,
    fontWeight: "800",
    marginBottom: 10
  },
  state: {
    fontSize: 32,
    fontWeight: "900",
    marginTop: 6
  },
  stateNormal: {
    color: "#0b6fb3"
  },
  stateError: {
    color: "#c62828"
  },
  meta: {
    color: "#60758b",
    fontSize: 13
  },
  progressTrack: {
    height: 14,
    borderRadius: 7,
    overflow: "hidden",
    backgroundColor: "#d8e4ef",
    marginTop: 16
  },
  progressFill: {
    height: "100%",
    backgroundColor: "#33a9e8"
  },
  progressRow: {
    flexDirection: "row",
    justifyContent: "space-between",
    marginTop: 8
  },
  commandGrid: {
    flexDirection: "row",
    flexWrap: "wrap",
    gap: 10
  },
  commandButton: {
    flexBasis: "48%",
    minHeight: 88,
    flexGrow: 1,
    borderRadius: 8,
    alignItems: "center",
    justifyContent: "center",
    gap: 6
  },
  buttonPrimary: {
    backgroundColor: "#0b6fb3"
  },
  buttonDanger: {
    backgroundColor: "#c62828"
  },
  buttonNeutral: {
    backgroundColor: "#ffffff",
    borderWidth: 1,
    borderColor: "#c6d4e3"
  },
  buttonText: {
    color: "#ffffff",
    fontSize: 16,
    fontWeight: "800"
  },
  buttonTextNeutral: {
    color: "#16324f",
    fontSize: 16,
    fontWeight: "800"
  },
  eventText: {
    color: "#16324f",
    fontSize: 16,
    marginTop: 8
  },
  sensorGrid: {
    flexDirection: "row",
    flexWrap: "wrap",
    gap: 8
  },
  pill: {
    minHeight: 34,
    paddingHorizontal: 12,
    borderRadius: 8,
    backgroundColor: "#e8eef6",
    alignItems: "center",
    justifyContent: "center"
  },
  pillActive: {
    backgroundColor: "#0b6fb3"
  },
  pillDanger: {
    backgroundColor: "#c62828"
  },
  pillText: {
    color: "#46627f",
    fontWeight: "700"
  },
  pillTextActive: {
    color: "#ffffff"
  },
  field: {
    marginBottom: 12
  },
  fieldLabel: {
    color: "#31475f",
    fontSize: 14,
    fontWeight: "700",
    marginBottom: 6
  },
  input: {
    minHeight: 46,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: "#c6d4e3",
    backgroundColor: "#f8fafc",
    paddingHorizontal: 12,
    color: "#16324f",
    fontSize: 16
  },
  switchRow: {
    minHeight: 48,
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
    borderTopWidth: 1,
    borderColor: "#eef3f8",
    paddingVertical: 8
  },
  eventRow: {
    flexDirection: "row",
    backgroundColor: "#ffffff",
    borderRadius: 8,
    padding: 12,
    borderWidth: 1,
    borderColor: "#dce5ef",
    gap: 12
  },
  eventTime: {
    width: 78,
    color: "#60758b",
    fontWeight: "700"
  },
  eventBody: {
    flex: 1
  },
  eventType: {
    color: "#0b6fb3",
    fontSize: 13,
    fontWeight: "800"
  },
  eventMessage: {
    color: "#16324f",
    marginTop: 3
  }
});
