import { useState, useEffect } from 'react'
import { onAuthStateChanged, signInWithEmailAndPassword, signOut } from 'firebase/auth'
import {
  collection, query, orderBy, limit, onSnapshot, doc, getDoc,
} from 'firebase/firestore'
import { auth, db } from './firebase.js'
import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
} from 'recharts'

// ─── Styles (inline — no build tooling dependency) ────────────────────────────
const s = {
  page:    { minHeight: '100vh', padding: '1.5rem', maxWidth: 480, margin: '0 auto' },
  card:    { background: '#fff', borderRadius: 16, padding: '1.5rem', marginBottom: '1rem',
             boxShadow: '0 1px 4px rgba(0,0,0,0.08)' },
  h1:      { fontSize: '1.75rem', fontWeight: 700, marginBottom: '1.5rem', color: '#1e40af' },
  h2:      { fontSize: '1.1rem', fontWeight: 600, marginBottom: '0.5rem', color: '#334155' },
  label:   { fontSize: '0.8rem', color: '#64748b', marginBottom: '0.25rem', display: 'block' },
  input:   { width: '100%', padding: '0.75rem', fontSize: '1rem', borderRadius: 8,
             border: '1.5px solid #cbd5e1', marginBottom: '0.75rem' },
  btn:     { width: '100%', padding: '0.85rem', fontSize: '1rem', fontWeight: 600,
             borderRadius: 8, border: 'none', cursor: 'pointer',
             background: '#1e40af', color: '#fff' },
  btnSm:   { padding: '0.4rem 0.9rem', fontSize: '0.85rem', borderRadius: 6,
             border: 'none', cursor: 'pointer', background: '#e2e8f0', color: '#334155' },
  pct:     (pct) => ({
    fontSize: '3rem', fontWeight: 800,
    color: pct > 40 ? '#16a34a' : pct > 20 ? '#d97706' : '#dc2626',
  }),
  bar:     { height: 18, background: '#e2e8f0', borderRadius: 9, overflow: 'hidden',
             marginBottom: '0.4rem' },
  barFill: (pct) => ({
    height: '100%', borderRadius: 9,
    width: `${pct}%`,
    background: pct > 40 ? '#16a34a' : pct > 20 ? '#d97706' : '#dc2626',
    transition: 'width 0.6s ease',
  }),
  ts:      { fontSize: '0.78rem', color: '#94a3b8' },
  error:   { color: '#dc2626', fontSize: '0.85rem', marginBottom: '0.5rem' },
}

// ─── Login screen ─────────────────────────────────────────────────────────────
function LoginScreen() {
  const [email, setEmail]   = useState('')
  const [pass, setPass]     = useState('')
  const [error, setError]   = useState('')
  const [loading, setLoading] = useState(false)

  const submit = async (e) => {
    e.preventDefault()
    setError('')
    setLoading(true)
    try {
      await signInWithEmailAndPassword(auth, email, pass)
    } catch {
      setError('Incorrect email or password.')
    } finally {
      setLoading(false)
    }
  }

  return (
    <div style={s.page}>
      <h1 style={s.h1}>Tank Monitor</h1>
      <div style={s.card}>
        <form onSubmit={submit}>
          <label style={s.label}>Email</label>
          <input style={s.input} type="email" value={email}
            onChange={e => setEmail(e.target.value)} autoComplete="email" />
          <label style={s.label}>Password</label>
          <input style={s.input} type="password" value={pass}
            onChange={e => setPass(e.target.value)} autoComplete="current-password" />
          {error && <p style={s.error}>{error}</p>}
          <button style={s.btn} disabled={loading}>
            {loading ? 'Signing in…' : 'Sign In'}
          </button>
        </form>
      </div>
    </div>
  )
}

// ─── Tank card (dashboard) ────────────────────────────────────────────────────
function TankCard({ deviceId, onClick }) {
  const [data, setData] = useState(null)

  useEffect(() => {
    // Listen to the most recent reading for this device
    const q = query(
      collection(db, 'devices', deviceId, 'readings'),
      orderBy('timestamp', 'desc'),
      limit(1)
    )
    return onSnapshot(q, snap => {
      if (!snap.empty) {
        const d = snap.docs[0].data()
        setData({ level: d.levelPercent, ts: d.timestamp?.toDate() })
      }
    })
  }, [deviceId])

  const pct = data ? Math.round(data.level) : null
  const tsStr = data?.ts
    ? data.ts.toLocaleString('en-AU', { hour: '2-digit', minute: '2-digit',
        day: 'numeric', month: 'short' })
    : 'Waiting…'

  return (
    <div style={{ ...s.card, cursor: 'pointer' }} onClick={onClick}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
        <h2 style={s.h2}>{deviceId}</h2>
        {pct !== null && <span style={s.pct(pct)}>{pct}%</span>}
      </div>
      {pct !== null && (
        <>
          <div style={s.bar}><div style={s.barFill(pct)} /></div>
          <span style={s.ts}>Updated {tsStr}</span>
        </>
      )}
      {pct === null && <span style={s.ts}>No readings yet</span>}
    </div>
  )
}

// ─── Tank detail screen ───────────────────────────────────────────────────────
function TankDetail({ deviceId, onBack }) {
  const [readings, setReadings] = useState([])

  useEffect(() => {
    const q = query(
      collection(db, 'devices', deviceId, 'readings'),
      orderBy('timestamp', 'desc'),
      limit(48)   // ~12 hours at 15-min intervals
    )
    return onSnapshot(q, snap => {
      const rows = snap.docs
        .map(d => ({
          level: Math.round(d.data().levelPercent),
          time:  d.data().timestamp?.toDate(),
        }))
        .filter(r => r.time)
        .reverse()
        .map(r => ({
          ...r,
          label: r.time.toLocaleTimeString('en-AU', { hour: '2-digit', minute: '2-digit' }),
        }))
      setReadings(rows)
    })
  }, [deviceId])

  const latest = readings[readings.length - 1]

  return (
    <div style={s.page}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '0.75rem', marginBottom: '1.5rem' }}>
        <button style={s.btnSm} onClick={onBack}>← Back</button>
        <h1 style={{ ...s.h1, marginBottom: 0 }}>{deviceId}</h1>
      </div>

      {latest && (
        <div style={s.card}>
          <div style={{ textAlign: 'center', marginBottom: '0.75rem' }}>
            <span style={s.pct(latest.level)}>{latest.level}%</span>
          </div>
          <div style={s.bar}><div style={s.barFill(latest.level)} /></div>
        </div>
      )}

      <div style={s.card}>
        <h2 style={{ ...s.h2, marginBottom: '1rem' }}>Last 12 hours</h2>
        {readings.length > 1 ? (
          <ResponsiveContainer width="100%" height={200}>
            <LineChart data={readings}>
              <CartesianGrid strokeDasharray="3 3" stroke="#e2e8f0" />
              <XAxis dataKey="label" tick={{ fontSize: 11 }} interval="preserveStartEnd" />
              <YAxis domain={[0, 100]} tick={{ fontSize: 11 }} unit="%" width={36} />
              <Tooltip formatter={v => [`${v}%`, 'Level']} />
              <Line type="monotone" dataKey="level" stroke="#1e40af"
                dot={false} strokeWidth={2} />
            </LineChart>
          </ResponsiveContainer>
        ) : (
          <p style={s.ts}>Not enough data yet</p>
        )}
      </div>
    </div>
  )
}

// ─── Dashboard ────────────────────────────────────────────────────────────────
function Dashboard({ user }) {
  const [devices, setDevices]   = useState([])
  const [selected, setSelected] = useState(null)

  useEffect(() => {
    // Load the list of devices belonging to this user
    const userDocRef = doc(db, 'users', user.uid)
    getDoc(userDocRef).then(snap => {
      if (snap.exists()) setDevices(snap.data().devices ?? [])
    })
  }, [user.uid])

  if (selected) {
    return <TankDetail deviceId={selected} onBack={() => setSelected(null)} />
  }

  return (
    <div style={s.page}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center',
          marginBottom: '1.5rem' }}>
        <h1 style={{ ...s.h1, marginBottom: 0 }}>My Tanks</h1>
        <button style={s.btnSm} onClick={() => signOut(auth)}>Sign out</button>
      </div>

      {devices.length === 0 && (
        <div style={s.card}>
          <p style={s.ts}>No devices yet. Add a device ID to your Firestore user document.</p>
        </div>
      )}
      {devices.map(id => (
        <TankCard key={id} deviceId={id} onClick={() => setSelected(id)} />
      ))}
    </div>
  )
}

// ─── Root ─────────────────────────────────────────────────────────────────────
export default function App() {
  const [user, setUser]       = useState(undefined)  // undefined = loading

  useEffect(() => {
    return onAuthStateChanged(auth, u => setUser(u ?? null))
  }, [])

  if (user === undefined) return null  // Brief auth check — no flash
  if (user === null)      return <LoginScreen />
  return <Dashboard user={user} />
}
