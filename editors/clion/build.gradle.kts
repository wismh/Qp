plugins {
    id("java")
    id("org.jetbrains.intellij.platform") version "2.18.1"
}

group = "com.qplus"
version = "0.3.1"

java {
    sourceCompatibility = JavaVersion.VERSION_21
    targetCompatibility = JavaVersion.VERSION_21
}

repositories {
    mavenCentral()
    intellijPlatform {
        defaultRepositories()
    }
}

dependencies {
    intellijPlatform {
        clion("2026.1.2")
    }
}

intellijPlatform {
    pluginConfiguration {
        ideaVersion {
            sinceBuild = "261"
        }
    }
}

tasks.named("buildPlugin") {
    doLast {
        val dest = rootDir.resolve("../../build")
        dest.mkdirs()
        copy {
            from(layout.buildDirectory.dir("distributions"))
            include("*.zip")
            into(dest)
        }
    }
}
