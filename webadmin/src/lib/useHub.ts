import { useCallback, useEffect, useRef, useState } from "react";
import { parseSnapshot } from "./status";
import type { HubSnapshot } from "./status";
import { observeSearch } from "./search-monitoring";
import type { SearchObservation } from "./search-monitoring";
import { observeTransfers } from "./transfer-monitoring";
import type { TransferObservation } from "./transfer-monitoring";
export type Connection = "connecting" | "live" | "unconfigured" | "offline";
export interface Sample {
  time: number;
  active: number;
}
export function useHub(autoRefresh: boolean) {
  const [connection, setConnection] = useState<Connection>("connecting");
  const [snapshot, setSnapshot] = useState<HubSnapshot | null>(null);
  const [refreshing, setRefreshing] = useState(false);
  const [checked, setChecked] = useState<number | null>(null);
  const [samples, setSamples] = useState<Sample[]>([]);
  const [searchSamples, setSearchSamples] = useState<SearchObservation[]>([]);
  const [transferSamples, setTransferSamples] = useState<TransferObservation[]>([]);
  const active = useRef<AbortController | null>(null);
  const previousUptime = useRef(0);
  const refresh = useCallback(async () => {
    active.current?.abort();
    const controller = new AbortController();
    active.current = controller;
    const timer = setTimeout(() => controller.abort("timeout"), 4000);
    setRefreshing(true);
    try {
      const response = await fetch("./admin-api/v1/status", {
        signal: controller.signal,
        cache: "no-store",
        credentials: "same-origin",
        redirect: "error",
      });
      if (controller.signal.aborted || active.current !== controller) return;
      if (!response.ok) {
        setConnection(response.status === 503 ? "unconfigured" : "offline");
        setSnapshot(null);
        setSamples([]);
        setSearchSamples([]);
        setTransferSamples([]);
        return;
      }
      if (!response.headers.get("content-type")?.startsWith("application/json"))
        throw new Error("Invalid content type");
      const reader = response.body?.getReader();
      if (!reader) throw new Error("Missing body");
      const decoder = new TextDecoder();
      let text = "",
        size = 0;
      try {
        while (true) {
          const chunk = await reader.read();
          if (chunk.done) break;
          size += chunk.value.length;
          if (size > 16384) throw new Error("Response too large");
          text += decoder.decode(chunk.value, { stream: true });
        }
        text += decoder.decode();
      } finally {
        await reader.cancel();
      }
      const value = parseSnapshot(JSON.parse(text));
      if (controller.signal.aborted) return;
      const now = Date.now();
      const restarted = value.server.uptime_seconds < previousUptime.current;
      previousUptime.current = value.server.uptime_seconds;
      setSnapshot(value);
      setConnection("live");
      setChecked(now);
      const observedAt = performance.now();
      setSearchSamples(previous => observeSearch(previous, value, observedAt));
      setTransferSamples(previous => observeTransfers(previous, value, observedAt));
      setSamples((previous) =>
        [
          ...(restarted ? [] : previous),
          { time: now, active: value.network.active_connections },
        ].slice(-30),
      );
    } catch {
      if (
        active.current === controller &&
        controller.signal.reason !== undefined &&
        controller.signal.reason !== "timeout"
      )
        return;
      if (active.current === controller) {
        setConnection("offline");
        setSnapshot(null);
        setSamples([]);
        setSearchSamples([]);
        setTransferSamples([]);
      }
    } finally {
      clearTimeout(timer);
      if (active.current === controller) setRefreshing(false);
    }
  }, []);
  useEffect(() => {
    void refresh();
    return () => active.current?.abort();
  }, [refresh]);
  useEffect(() => {
    if (!autoRefresh) return;
    const timer = setInterval(() => void refresh(), 5000);
    return () => clearInterval(timer);
  }, [autoRefresh, refresh]);
  const clearSearchSamples = useCallback(() => setSearchSamples([]), []);
  const clearTransferSamples = useCallback(() => setTransferSamples([]), []);
  return { connection, snapshot, refreshing, checked, samples, searchSamples, clearSearchSamples, transferSamples, clearTransferSamples, refresh };
}
