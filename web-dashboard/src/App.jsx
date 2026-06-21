import { useState, useEffect } from 'react';
import mqtt from 'mqtt';
import { Activity, Database } from 'lucide-react';
import Plant1Card from './components/Plant1Card';
import Plant2Card from './components/Plant2Card';
import AlertLog from './components/AlertLog';

const BROKER_URL = 'wss://704e8cb56cf1497ca1ff0e371b9415ff.s1.eu.hivemq.cloud:8884/mqtt';
const OPTIONS = {
  username: 'kompak',
  password: 'Kompak2026',
  clientId: `web_dashboard_${Math.random().toString(16).slice(3)}`,
  clean: true,
  reconnectPeriod: 3000,
};

// Define Topics
const TOPICS = {
  P1_PH: 'wwtp/plant1/ph',
  P1_LEVEL: 'wwtp/plant1/level',
  P1_VALVE: 'wwtp/plant1/valve/state',
  P1_MIXER: 'wwtp/plant1/mixer/state',
  P1_ACID: 'wwtp/plant1/dosing/acid/state',
  P1_BASE: 'wwtp/plant1/dosing/base/state',
  P2_LEVEL: 'wwtp/plant2/level',
  P2_FLOW: 'wwtp/plant2/flow',
  P2_VALVE_MAIN: 'wwtp/plant2/valve/main/state',
  P2_VALVE_FS: 'wwtp/plant2/valve/failsafe/state',
  MTU_DOSING: 'wwtp/mtu/dosing/state',
  ALERT: 'wwtp/alert'
};

function App() {
  const [client, setClient] = useState(null);
  const [connectStatus, setConnectStatus] = useState('Disconnected');
  
  // State
  const [p1Data, setP1Data] = useState({ ph: 0, levelPct: 0, levelL: 0, valveOpen: false, mixerOpen: false, acidOpen: false, baseOpen: false });
  const [p2Data, setP2Data] = useState({ levelPct: 0, levelL: 0, flowLPM: 0, mainValve: false, fsValve: false });
  const [mtuState, setMtuState] = useState('MONITORING');
  const [alerts, setAlerts] = useState([]);

  useEffect(() => {
    console.log('Connecting to MQTT Broker...');
    setConnectStatus('Connecting...');
    const mqttClient = mqtt.connect(BROKER_URL, OPTIONS);

    mqttClient.on('connect', () => {
      setConnectStatus('Connected');
      console.log('Connected!');
      
      // Subscribe to all topics
      Object.values(TOPICS).forEach(topic => {
        mqttClient.subscribe(topic, (err) => {
          if (err) console.error('Sub error:', err);
        });
      });
    });

    mqttClient.on('error', (err) => {
      console.error('MQTT Error: ', err);
      mqttClient.end();
    });

    mqttClient.on('reconnect', () => {
      setConnectStatus('Reconnecting...');
    });

    mqttClient.on('message', (topic, message) => {
      const payload = message.toString();
      try {
        const data = JSON.parse(payload);
        
        // Route messages
        switch(topic) {
          case TOPICS.P1_PH:
            setP1Data(prev => ({ ...prev, ph: data.value }));
            break;
          case TOPICS.P1_LEVEL:
            setP1Data(prev => ({ ...prev, levelPct: data.value_pct, levelL: data.value_liter }));
            break;
          case TOPICS.P1_VALVE:
            setP1Data(prev => ({ ...prev, valveOpen: data.state === 'OPEN' }));
            break;
          case TOPICS.P1_MIXER:
            setP1Data(prev => ({ ...prev, mixerOpen: data.state === 'ON' }));
            break;
          case TOPICS.P1_ACID:
            setP1Data(prev => ({ ...prev, acidOpen: data.state === 'ON' }));
            break;
          case TOPICS.P1_BASE:
            setP1Data(prev => ({ ...prev, baseOpen: data.state === 'ON' }));
            break;
          case TOPICS.P2_LEVEL:
            setP2Data(prev => ({ ...prev, levelPct: data.value_pct, levelL: data.value_liter }));
            break;
          case TOPICS.P2_FLOW:
            setP2Data(prev => ({ ...prev, flowLPM: data.flow_LPM }));
            break;
          case TOPICS.P2_VALVE_MAIN:
            setP2Data(prev => ({ ...prev, mainValve: data.state === 'OPEN' }));
            break;
          case TOPICS.P2_VALVE_FS:
            setP2Data(prev => ({ ...prev, fsValve: data.state === 'OPEN' }));
            break;
          case TOPICS.MTU_DOSING:
            setMtuState(data.state || payload);
            break;
          case TOPICS.ALERT:
            setAlerts(prev => {
              const newAlerts = [{ time: new Date().toLocaleTimeString(), msg: data.message || payload, type: data.code || 'ALERT' }, ...prev];
              return newAlerts.slice(0, 50); // keep last 50
            });
            break;
          default:
            break;
        }
      } catch (e) {
        // If not JSON, handle as plain text
        if (topic === TOPICS.MTU_DOSING) setMtuState(payload);
      }
    });

    setClient(mqttClient);

    return () => {
      if (mqttClient) {
        mqttClient.end();
      }
    };
  }, []);

  const publishCommand = (topic, payload) => {
    if (client && client.connected) {
      client.publish(topic, JSON.stringify(payload));
      console.log(`Published to ${topic}:`, payload);
    } else {
      alert("MQTT is not connected!");
    }
  };

    const testDatabaseInsert = () => {
      const payload = {
        message: `Test data from web via MQTT at ${new Date().toLocaleTimeString()}`
      };
      publishCommand('wwtp/test', payload);
      alert("Pesan test telah dikirim ke topik MQTT 'wwtp/test'. Silakan cek Node-RED dan Supabase.");
    };

  return (
    <>
      <div className="header">
        <div>
          <h1>SCADA WWTP Monitor</h1>
          <p className="text-muted">Real-time telemetry via MQTT WebSockets</p>
        </div>
        <div style={{ display: 'flex', gap: '1rem', alignItems: 'center' }}>
          <button 
            className="ctrl-btn" 
            style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', backgroundColor: '#3b82f6' }}
            onClick={testDatabaseInsert}
          >
            <Database size={16} /> Test Database
          </button>
          
          <div className={`status-badge ${connectStatus === 'Connected' ? 'connected' : 'disconnected'}`}>
            <div className={`status-indicator ${connectStatus === 'Connected' ? 'active pulse' : 'fault'}`}></div>
            {connectStatus}
          </div>
        </div>
      </div>

      <div className="dashboard-grid">
        <Plant1Card data={p1Data} mtuState={mtuState} publishCommand={publishCommand} />
        <Plant2Card data={p2Data} publishCommand={publishCommand} />
        <AlertLog alerts={alerts} mtuState={mtuState} />
      </div>
    </>
  );
}

export default App;
