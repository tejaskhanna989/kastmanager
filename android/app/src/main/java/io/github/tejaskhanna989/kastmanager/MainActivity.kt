/* Kast Manager for Android 2.8.0 — Tejas Khanna, GPL-3.0-or-later.
 * Minimal native companion: dashboard, processes, system, about. */
package io.github.tejaskhanna989.kastmanager

import android.app.ActivityManager
import android.app.AlertDialog
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.Uri
import android.os.BatteryManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.StatFs
import android.os.SystemClock
import android.view.View
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.ListView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.google.android.material.bottomnavigation.BottomNavigationView
import java.io.File

data class ProcRow(val pid: Int, val name: String, val rssMb: Long)

class MainActivity : AppCompatActivity() {

    private val tick = Handler(Looper.getMainLooper())
    private var lastTotal = 0L
    private var lastIdle = 0L
    private var haveLast = false
    private val procs = mutableListOf<ProcRow>()
    private lateinit var procAdapter: ArrayAdapter<String>
    private var currentTab = 0

    private fun readStat(): Pair<Long, Long>? {
        return try {
            val parts = File("/proc/stat").readLines().first()
                .trim().split(Regex("\\s+"))
            if (parts.size < 5 || parts[0] != "cpu") return null
            val nums = parts.drop(1).map { it.toLong() }
            val total = nums.sum()
            val idle = nums[3] + nums[4]
            total to idle
        } catch (_: Exception) { null }
    }

    private fun cpuPercent(): Double {
        val (total, idle) = readStat() ?: return 0.0
        var pct = 0.0
        if (haveLast && total > lastTotal) {
            val dt = total - lastTotal
            val di = (idle - lastIdle).coerceAtLeast(0)
            pct = (100.0 * (1.0 - di.toDouble() / dt)).coerceIn(0.0, 100.0)
        }
        lastTotal = total
        lastIdle = idle
        haveLast = true
        return pct
    }

    private fun fmtBytes(b: Long): String {
        if (b < 1024) return "$b B"
        val kb = b / 1024.0
        if (kb < 1024) return "%.1f KB".format(kb)
        val mb = kb / 1024.0
        if (mb < 1024) return "%.1f MB".format(mb)
        return "%.1f GB".format(mb / 1024.0)
    }

    private fun fmtUptime(): String {
        val s = SystemClock.elapsedRealtime() / 1000
        val d = s / 86400
        val h = (s % 86400) / 3600
        val m = (s % 3600) / 60
        return if (d > 0) "${d}d ${h}h" else if (h > 0) "${h}h ${m}m" else "${m}m"
    }

    private fun updateDash() {
        val am = getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        val mi = ActivityManager.MemoryInfo()
        am.getMemoryInfo(mi)
        val usedPct = if (mi.totalMem > 0)
            100.0 * (mi.totalMem - mi.availMem) / mi.totalMem else 0.0

        val stat = StatFs(filesDir.path)
        val free = stat.availableBytes
        val total = stat.totalBytes

        val batt = registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val level = batt?.getIntExtra(BatteryManager.EXTRA_LEVEL, -1) ?: -1
        val scale = batt?.getIntExtra(BatteryManager.EXTRA_SCALE, 100) ?: 100
        val pct = if (level >= 0) 100 * level / scale else -1
        val plug = batt?.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0) ?: 0

        findViewById<TextView>(R.id.dash_cpu).text = "%.1f%%".format(cpuPercent())
        findViewById<TextView>(R.id.dash_cpu_sub).text =
            "${Runtime.getRuntime().availableProcessors()} cores"
        findViewById<TextView>(R.id.dash_mem).text = "%.1f%%".format(usedPct)
        findViewById<TextView>(R.id.dash_mem_sub).text =
            "${fmtBytes(mi.totalMem - mi.availMem)} of ${fmtBytes(mi.totalMem)}"
        findViewById<TextView>(R.id.dash_storage).text = fmtBytes(total - free)
        findViewById<TextView>(R.id.dash_storage_sub).text = "${fmtBytes(free)} free"
        findViewById<TextView>(R.id.dash_batt).text =
            if (pct >= 0) "$pct%" else "n/a"
        findViewById<TextView>(R.id.dash_batt_sub).text =
            if (plug > 0) "charging" else "on battery"
        findViewById<TextView>(R.id.dash_uptime).text = fmtUptime()
    }

    private fun refreshProcs() {
        procs.clear()
        val myUid = android.os.Process.myUid()
        File("/proc").listFiles()?.forEach { dir ->
            val pid = dir.name.toIntOrNull() ?: return@forEach
            try {
                val status = File(dir, "status").readText()
                val uidLine = status.lineSequence()
                    .firstOrNull { it.startsWith("Uid:") } ?: return@forEach
                val uid = uidLine.split(Regex("\\s+"))[1].toInt()
                if (uid != myUid) return@forEach
                val cmdline = try {
                    File(dir, "cmdline").readBytes().decodeToString()
                        .split('\u0000').firstOrNull()?.substringAfterLast('/') ?: "?"
                } catch (_: Exception) { "?" }
                val statm = try {
                    File(dir, "statm").readText().trim().split(" ")
                } catch (_: Exception) { return@forEach }
                val rssMb = statm.getOrNull(1)?.toLongOrNull()
                    ?.times(4096) ?: 0L
                procs.add(ProcRow(pid, cmdline.ifEmpty { "?" }, rssMb / 1048576))
            } catch (_: Exception) { }
        }
        procs.sortByDescending { it.rssMb }
        procAdapter.clear()
        procAdapter.addAll(procs.map { "${it.name} (${it.pid}) · ${it.rssMb} MB" })
    }

    private fun fillSystem() {
        val box = findViewById<android.widget.LinearLayout>(R.id.sys_rows)
        box.removeAllViews()
        fun row(k: String, v: String) {
            val t = TextView(this)
            t.text = "$k: $v"
            t.setPadding(10, 10, 10, 10)
            t.setTextIsSelectable(true)
            box.addView(t)
        }
        row("Device", "${android.os.Build.MANUFACTURER} ${android.os.Build.MODEL}")
        row("Android", "${android.os.Build.VERSION.RELEASE} (SDK ${android.os.Build.VERSION.SDK_INT})")
        row("Board", "${android.os.Build.BOARD} ${android.os.Build.HARDWARE}")
        row("CPU ABIs", android.os.Build.SUPPORTED_ABIS.joinToString(", "))
        row("Cores", "${Runtime.getRuntime().availableProcessors()}")
        val am = getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        val mi = ActivityManager.MemoryInfo()
        am.getMemoryInfo(mi)
        row("Total RAM", fmtBytes(mi.totalMem))
        row("Uptime", fmtUptime())
    }

    private fun openUrl(url: String) {
        try {
            startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
        } catch (_: Exception) {
            Toast.makeText(this, "No browser found", Toast.LENGTH_SHORT).show()
        }
    }

    private val ticker = object : Runnable {
        override fun run() {
            if (currentTab == 0) updateDash()
            if (currentTab == 1) refreshProcs()
            tick.postDelayed(this, 2000)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val pages = listOf(
            findViewById<View>(R.id.page_dashboard),
            findViewById<View>(R.id.page_processes),
            findViewById<View>(R.id.page_system),
            findViewById<View>(R.id.page_about),
        )
        val nav = findViewById<BottomNavigationView>(R.id.bottom_nav)
        nav.setOnItemSelectedListener { item ->
            currentTab = when (item.itemId) {
                R.id.nav_dashboard -> 0
                R.id.nav_processes -> 1
                R.id.nav_system -> 2
                else -> 3
            }
            pages.forEachIndexed { i, v ->
                v.visibility = if (i == currentTab) View.VISIBLE else View.GONE
            }
            if (currentTab == 0) updateDash()
            if (currentTab == 1) refreshProcs()
            if (currentTab == 2) fillSystem()
            true
        }

        procAdapter = ArrayAdapter(this,
            android.R.layout.simple_list_item_1, mutableListOf<String>())
        val list = findViewById<ListView>(R.id.proc_list)
        list.adapter = procAdapter
        list.setOnItemClickListener { _, _, pos, _ ->
            val p = procs.getOrNull(pos) ?: return@setOnItemClickListener
            AlertDialog.Builder(this)
                .setTitle("End ${p.name}?")
                .setMessage("Kill PID ${p.pid}? Only your own processes can end.")
                .setPositiveButton("End") { _, _ ->
                    try {
                        android.os.Process.killProcess(p.pid)
                    } catch (_: Exception) { }
                    refreshProcs()
                }
                .setNegativeButton("Cancel", null)
                .show()
        }
        findViewById<Button>(R.id.proc_refresh).setOnClickListener { refreshProcs() }

        findViewById<Button>(R.id.about_github).setOnClickListener {
            openUrl("https://github.com/tejaskhanna989/kastmanager")
        }
        findViewById<Button>(R.id.about_donate_in).setOnClickListener {
            openUrl("https://razorpay.me/@deepakkhanna1473")
        }
        findViewById<Button>(R.id.about_donate_out).setOnClickListener {
            openUrl("https://www.paypal.com/paypalme/deepakrajkhanna/")
        }

        updateDash()
        tick.postDelayed(ticker, 2000)
    }

    override fun onDestroy() {
        tick.removeCallbacks(ticker)
        super.onDestroy()
    }
}
