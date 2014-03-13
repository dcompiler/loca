args <- commandArgs(TRUE)

program <- args[1]
ext <- args[2]

rawdata <- read.table(paste(program, ext, sep=""), skip=1)
N <- strsplit(strsplit(readLines(paste(program, ext, sep=""), 1), " ")[[1]][1], ":")[[1]][2]
M <- strsplit(strsplit(readLines(paste(program, ext, sep=""), 1), " ")[[1]][2], ":")[[1]][2]
M <- toString(as.numeric(M) * 64)

message <- paste("Length of Trace = ", N, "  ", "Data Size = ", M)

#draw footprint curve
saveto <- sprintf("%s.fp.png", program)
x <- rawdata[,2]
y <- rawdata[,3] * 64 # "* 64" converts to bytes
xlab <- "window size (accesses)"
ylab <- "footprint (bytes)"
legend <- c("average footprint")
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab,
	frame.plot=TRUE, col="red", main="Average Footprint")
legend("topleft", legend, lty=1, col="red", title=message)
dev.off()

#draw lifetime curve
saveto <- sprintf("%s.lf.png", program)
x <- rawdata[,3] * 64
y <- rawdata[,2]
xlab <- "cache size (bytes)"
ylab <- "lifetime (accesses)"
legend <- c("lifetime")
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab,
	frame.plot=TRUE, col="red", main="Data Lifetime in Cache")
legend("topleft", legend, lty=1, col="red", title=message)
dev.off()

#draw miss ratio curve
saveto <- sprintf("%s.mr.png", program)
x <- rawdata[,3] * 64
y <- rawdata[,5] * 100
xlab <- "cache size (bytes)"
ylab <- "miss ratio (percentage)"
legend <- c("miss ratio")
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab,
	frame.plot=TRUE, col="red", main="Miss Ratio Curve")
legend("topright", legend, lty=1, col="red", title=message)
dev.off()

#draw reuse signature
saveto <- sprintf("%s.rd.png", program)
x <- rawdata[,3] * 64
y <- rawdata[,4] * 100
xlab <- "reuse distance (bytes)"
ylab <- "percentage"
legend <- c("reuse signature")
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab,
	frame.plot=TRUE, col="red", main="Reuse Distance Distribution")
legend("topright", legend, lty=1, col="red", title=message)
dev.off()
